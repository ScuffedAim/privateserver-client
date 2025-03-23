#include "BanchoNetworking.h"

#include <time.h>

#include <mutex>
#include <thread>

#ifndef _MSC_VER
#include <unistd.h>
#endif

#include "Bancho.h"
#include "BanchoLeaderboard.h"
#include "BanchoProtocol.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "CBaseUICheckbox.h"
#include "Chat.h"
#include "ConVar.h"
#include "Database.h"
#include "Downloader.h"
#include "Engine.h"
#include "File.h"
#include "Lobby.h"
#include "OptionsMenu.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "SongBrowser/SongBrowser.h"
#include "UIAvatar.h"
#include "UIButton.h"
#include "miniz.h"

// Bancho protocol
std::mutex outgoing_mutex;
bool try_logging_in = false;
Packet login_packet;
Packet outgoing;
std::mutex incoming_mutex;
std::vector<Packet> incoming_queue;
time_t last_packet_tms = {0};
double seconds_between_pings = 1.0;

std::atomic<bool> dead = true;

std::string auth_header = "";
UString cho_token = "";

// osu! private API
std::mutex api_requests_mutex;
std::mutex api_responses_mutex;
std::vector<APIRequest> api_request_queue;
std::vector<Packet> api_response_queue;

// dummy method to prevent curl from printing to stdout
size_t curldummy(void *buffer, size_t size, size_t nmemb, void *userp) {
    (void)buffer;
    (void)userp;
    return size * nmemb;
}

void disconnect() {
    std::lock_guard<std::mutex> lock(outgoing_mutex);

    // Logout
    // This is a blocking call, but we *do* want this to block when quitting the game.
    if(bancho.is_online()) {
        Packet packet;
        write<u16>(&packet, LOGOUT);
        write<u8>(&packet, 0);
        write<u32>(&packet, 0);

        CURL *curl = curl_easy_init();
        auto version_header = UString::format("x-mcosu-ver: %s", bancho.neosu_version.toUtf8());
        struct curl_slist *chunk = NULL;
        chunk = curl_slist_append(chunk, auth_header.c_str());
        chunk = curl_slist_append(chunk, version_header.toUtf8());
        auto scheme = cv_use_https.getBool() ? "https://" : "http://";
        auto query_url = UString::format("%sc.%s/", scheme, bancho.endpoint.toUtf8());
        curl_easy_setopt(curl, CURLOPT_URL, query_url.toUtf8());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, packet.memory);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, packet.pos);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "osu!");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curldummy);
#ifdef _WIN32
        // ABSOLUTELY RETARDED, FUCK WINDOWS
        curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
#endif
        curl_easy_perform(curl);
        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);
        free(packet.memory);
    }

    try_logging_in = false;
    auth_header = "";
    free(outgoing.memory);
    outgoing = Packet();

    bancho.user_id = 0;
    bancho.spectated_player_id = 0;
    bancho.spectators.clear();
    bancho.fellow_spectators.clear();
    bancho.server_icon_url = "";
    if(bancho.server_icon != NULL) {
        engine->getResourceManager()->destroyResource(bancho.server_icon);
        bancho.server_icon = NULL;
    }

    std::vector<ConVar *> convars = convar->getConVarArray();
    for(auto var : convars) {
        var->resetDefaults();
    }

    bancho.score_submission_policy = ServerPolicy::YES;
    osu->optionsMenu->updateLayout();

    osu->optionsMenu->logInButton->setText("Log in");
    osu->optionsMenu->logInButton->setColor(0xff00ff00);
    osu->optionsMenu->logInButton->is_loading = false;

    for(auto pair : online_users) {
        delete pair.second;
    }
    online_users.clear();
    friends.clear();

    osu->chat->onDisconnect();

    // XXX: We should toggle between "offline" sorting options and "online" ones
    //      Online ones would be "Local scores", "Global", "Country", "Selected mods" etc
    //      While offline ones would be "By score", "By pp", etc
    osu->songBrowser2->onSortScoresChange(UString("Sort by pp"), 0);

    abort_downloads();
}

void reconnect() {
    disconnect();

    // Disable autologin, in case there's an error while logging in
    // Will be reenabled after the login succeeds
    cv_mp_autologin.setValue(false);

    bancho.username = cv_name.getString();
    bancho.endpoint = cv_mp_server.getString();

    // Admins told me they don't want any clients to connect
    const char *server_blacklist[] = {
        "ppy.sh",  // haven't asked, but the answer is obvious
        "gatari.pw",
    };
    for(const char *endpoint : server_blacklist) {
        if(!strcmp(endpoint, bancho.endpoint.toUtf8())) {
            osu->notificationOverlay->addToast("This server does not allow neosu clients.");
            return;
        }
    }

    UString password = cv_mp_password.getString();
    if(password.length() == 0) {
        // No password: don't try to log in
        return;
    }
    const char *pw = password.toUtf8();  // password needs to stay in scope!
    bancho.pw_md5 = md5((u8 *)pw, strlen(pw));

    // Admins told me they don't want score submission enabled
    const char *submit_blacklist[] = {
        "akatsuki.gg",
        "ripple.moe",
    };
    for(const char *endpoint : submit_blacklist) {
        if(!strcmp(endpoint, bancho.endpoint.toUtf8())) {
            bancho.score_submission_policy = ServerPolicy::NO;
            break;
        }
    }

    osu->optionsMenu->logInButton->is_loading = true;
    Packet new_login_packet = build_login_packet();

    outgoing_mutex.lock();
    free(login_packet.memory);
    login_packet = new_login_packet;
    try_logging_in = true;
    outgoing_mutex.unlock();
}

size_t curl_write(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Packet *mem = (Packet *)userp;

    u8 *ptr = (u8 *)realloc(mem->memory, mem->size + realsize + 1);
    if(!ptr) {
        /* out of memory! */
        debugLog("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static void send_api_request(CURL *curl, APIRequest api_out) {
    // XXX: Use download()

    Packet response;
    response.id = api_out.type;
    response.extra = api_out.extra;
    response.extra_int = api_out.extra_int;
    response.memory = (u8 *)malloc(2048);

    struct curl_slist *chunk = NULL;
    auto scheme = cv_use_https.getBool() ? "https://" : "http://";
    auto query_url = UString::format("%sosu.%s%s", scheme, bancho.endpoint.toUtf8(), api_out.path.toUtf8());
    curl_easy_setopt(curl, CURLOPT_URL, query_url.toUtf8());
    if(api_out.type == SUBMIT_SCORE) {
        auto token_header = UString::format("token: %s", cho_token.toUtf8());
        chunk = curl_slist_append(chunk, token_header.toUtf8());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
    }
    if(api_out.mime != NULL) {
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, api_out.mime);
    }
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "osu!");
#ifdef _WIN32
    // ABSOLUTELY RETARDED, FUCK WINDOWS
    curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
#endif

    CURLcode res = curl_easy_perform(curl);
    if(res == CURLE_OK) {
        api_responses_mutex.lock();
        api_response_queue.push_back(response);
        api_responses_mutex.unlock();
    }

    if(api_out.mime) {
        curl_mime_free(api_out.mime);
    }
    curl_easy_reset(curl);
    curl_slist_free_all(chunk);
}

static void send_bancho_packet(CURL *curl, Packet outgoing) {
    Packet response;
    response.memory = (u8 *)malloc(2048);

    struct curl_slist *chunk = NULL;
    auto version_header = UString::format("x-mcosu-ver: %s", bancho.neosu_version.toUtf8());
    chunk = curl_slist_append(chunk, version_header.toUtf8());
    if(!auth_header.empty()) {
        chunk = curl_slist_append(chunk, auth_header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    auto scheme = cv_use_https.getBool() ? "https://" : "http://";
    auto query_url = UString::format("%sc.%s/", scheme, bancho.endpoint.toUtf8());
    curl_easy_setopt(curl, CURLOPT_URL, query_url.toUtf8());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, outgoing.memory);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, outgoing.pos);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "osu!");
#ifdef _WIN32
    // ABSOLUTELY RETARDED, FUCK WINDOWS
    curl_easy_setopt(curl, CURLOPT_CAINFO, "curl-ca-bundle.crt");
#endif

    last_packet_tms = time(NULL);
    CURLcode res = curl_easy_perform(curl);
    CURLHcode hres;
    if(res != CURLE_OK) {
        debugLog("Failed to send packet, cURL error %d: %s\n", res, curl_easy_strerror(res));
        if(auth_header.empty()) {
            // XXX: Not thread safe, playing with fire here
            auto errmsg = UString::format("Failed to log in: %s", curl_easy_strerror(res));
            osu->getNotificationOverlay()->addToast(errmsg);
        }
        goto end;
    }

    // Update auth token if applicable
    struct curl_header *header;
    hres = curl_easy_header(curl, "cho-token", 0, CURLH_HEADER, -1, &header);
    if(hres == CURLHE_OK) {
        auth_header = "osu-token: " + std::string(header->value);
        cho_token = UString(header->value);
    }

    hres = curl_easy_header(curl, "x-mcosu-features", 0, CURLH_HEADER, -1, &header);
    if(hres == CURLHE_OK) {
        if(strstr(header->value, "submit=0") != NULL) {
            bancho.score_submission_policy = ServerPolicy::NO;
            debugLog("Server doesn't want score submission. :(\n");
        } else if(strstr(header->value, "submit=1") != NULL) {
            bancho.score_submission_policy = ServerPolicy::YES;
            debugLog("Server wants score submission! :D\n");
        }
    }

    while(response.pos < response.size) {
        u16 packet_id = read<u16>(&response);
        response.pos++;
        u32 packet_len = read<u32>(&response);
        if(packet_len > 10485760) {
            debugLog("Received a packet over 10Mb! Dropping response.\n");
            goto end;
        }

        Packet incoming = {
            .id = packet_id,
            .memory = (u8 *)malloc(packet_len),
            .size = packet_len,
            .pos = 0,
        };
        memcpy(incoming.memory, response.memory + response.pos, packet_len);

        incoming_mutex.lock();
        incoming_queue.push_back(incoming);
        incoming_mutex.unlock();

        seconds_between_pings = 1.0;
        response.pos += packet_len;
    }

end:
    curl_easy_reset(curl);
    free(response.memory);
    curl_slist_free_all(chunk);
}

static void *do_networking() {
    last_packet_tms = time(NULL);

    CURL *curl = curl_easy_init();
    if(!curl) {
        debugLog("Failed to initialize cURL, online functionality disabled.\n");
        return NULL;
    }

    while(!dead.load()) {
        api_requests_mutex.lock();
        if(api_request_queue.empty()) {
            api_requests_mutex.unlock();
        } else {
            APIRequest api_out = api_request_queue.front();
            api_request_queue.erase(api_request_queue.begin());
            api_requests_mutex.unlock();

            send_api_request(curl, api_out);
        }

        if(osu && osu->lobby->isVisible()) seconds_between_pings = 1;
        if(bancho.spectated_player_id != 0) seconds_between_pings = 1;
        if(bancho.is_in_a_multi_room() && seconds_between_pings > 3) seconds_between_pings = 3;
        bool should_ping = difftime(time(NULL), last_packet_tms) > seconds_between_pings;
        if(bancho.user_id <= 0) should_ping = false;

        outgoing_mutex.lock();
        if(try_logging_in) {
            Packet login = login_packet;
            login_packet = Packet();
            try_logging_in = false;
            outgoing_mutex.unlock();
            send_bancho_packet(curl, login);
            free(login.memory);
            outgoing_mutex.lock();
        } else if(should_ping && outgoing.pos == 0) {
            write<u16>(&outgoing, PING);
            write<u8>(&outgoing, 0);
            write<u32>(&outgoing, 0);

            // Polling gets slower over time, but resets when we receive new data
            if(seconds_between_pings < 30.0) {
                seconds_between_pings += 1.0;
            }
        }

        if(outgoing.pos > 0) {
            Packet out = outgoing;
            outgoing = Packet();
            outgoing_mutex.unlock();

            // DEBUG: If we're not sending the right amount of bytes, bancho.py just
            // chugs along! To try to detect it faster, we'll send two packets per request.
            write<u16>(&out, PING);
            write<u8>(&out, 0);
            write<u32>(&out, 0);

            send_bancho_packet(curl, out);
            free(out.memory);
        } else {
            outgoing_mutex.unlock();
        }

        env->sleep(1000);  // wait 1ms
    }

    curl_easy_cleanup(curl);

    return NULL;
}

static void handle_api_response(Packet packet) {
    switch(packet.id) {
        case GET_BEATMAPSET_INFO: {
            process_beatmapset_info_response(packet);
            break;
        }

        case GET_MAP_LEADERBOARD: {
            process_leaderboard_response(packet);
            break;
        }

        case GET_REPLAY: {
            if(packet.size == 0) {
                // Most likely, 404
                osu->notificationOverlay->addToast("Failed to download replay");
                break;
            }

            FinishedScore *score = (FinishedScore *)packet.extra;
            auto replay_path = UString::format(MCENGINE_DATA_DIR "replays/%s/%d.replay.lzma", score->server.c_str(),
                                               score->unixTimestamp);

            // XXX: this is blocking main thread
            FILE *replay_file = fopen(replay_path.toUtf8(), "wb");
            if(replay_file == NULL) {
                osu->notificationOverlay->addToast("Failed to save replay");
                break;
            }

            fwrite(packet.memory, packet.size, 1, replay_file);
            fclose(replay_file);
            LegacyReplay::load_and_watch(*score);
            break;
        }

        case MARK_AS_READ: {
            // (nothing to do)
            break;
        }

        case SUBMIT_SCORE: {
            // TODO @kiwec: handle response
            debugLog("Score submit result: %s\n", packet.memory);

            // Reset leaderboards so new score will appear
            db->online_scores.clear();
            osu->getSongBrowser()->rebuildScoreButtons();
            break;
        }

        default: {
            // NOTE: API Response type is same as API Request type
            debugLog("No handler for API response type %d!\n", packet.id);
        }
    }
}

void receive_api_responses() {
    std::lock_guard<std::mutex> lock(api_responses_mutex);
    while(!api_response_queue.empty()) {
        Packet incoming = api_response_queue.front();
        api_response_queue.erase(api_response_queue.begin());
        handle_api_response(incoming);
        free(incoming.memory);
        free(incoming.extra);
    }
}

void receive_bancho_packets() {
    std::lock_guard<std::mutex> lock(incoming_mutex);
    while(!incoming_queue.empty()) {
        Packet incoming = incoming_queue.front();
        incoming_queue.erase(incoming_queue.begin());
        handle_packet(&incoming);
        free(incoming.memory);
    }
}

void send_api_request(APIRequest request) {
    if(bancho.user_id <= 0) {
        debugLog("Cannot send API request of type %d since we are not logged in.\n", request.type);
        return;
    }

    std::lock_guard<std::mutex> lock(api_requests_mutex);

    // Jank way to do things... remove outdated requests now
    api_request_queue.erase(std::remove_if(api_request_queue.begin(), api_request_queue.end(),
                                           [request](APIRequest r) { return r.type = request.type; }),
                            api_request_queue.end());

    api_request_queue.push_back(request);
}

void send_packet(Packet &packet) {
    if(bancho.user_id <= 0) {
        // Don't queue any packets until we're logged in
        free(packet.memory);
        packet.memory = NULL;
        packet.size = 0;
        return;
    }

    // debugLog("Sending packet of type %hu: ", packet.id);
    // for (int i = 0; i < packet.pos; i++) {
    //     debugLog("%02x ", packet.memory[i]);
    // }
    // debugLog("\n");

    outgoing_mutex.lock();

    // We're not sending it immediately, instead we just add it to the pile of
    // packets to send
    write<u16>(&outgoing, packet.id);
    write<u8>(&outgoing, 0);
    write<u32>(&outgoing, packet.pos);
    write_bytes(&outgoing, packet.memory, packet.pos);

    outgoing_mutex.unlock();

    free(packet.memory);
    packet.memory = NULL;
    packet.size = 0;
}

void init_networking_thread() {
    dead = false;
    auto net_thread = std::thread(do_networking);
    net_thread.detach();
}

void kill_networking_thread() { dead = true; }
