#include <time.h>
#include <unistd.h>
#include "miniz.h"

#include "Bancho.h"
#include "BanchoLeaderboard.h"
#include "BanchoNetworking.h"
#include "BanchoProtocol.h"
#include "ConVar.h"
#include "Engine.h"
#include "OsuOptionsMenu.h"
#include "OsuRoom.h"
#include "OsuUIButton.h"

#ifdef MCENGINE_FEATURE_PTHREADS
#include <curl/curl.h>
#include <pthread.h>
#include "File.h"

pthread_t networking_thread;

// TODO @kiwec: improve the way the client logs in and manages logged in state
// Right now if log in fails or expires, things explode

// TODO @kiwec: we should be able to cancel pending API requests, eg for leaderboards

// Bancho protocol
pthread_mutex_t outgoing_mutex = PTHREAD_MUTEX_INITIALIZER;
bool try_logging_in = false;
Packet login_packet = {0};
Packet outgoing = {0};
pthread_mutex_t incoming_mutex = PTHREAD_MUTEX_INITIALIZER;
std::vector<Packet> incoming_queue;
time_t last_packet_tms = {0};
double seconds_between_pings = 1.0;

std::string auth_header = "";

// osu! private API
pthread_mutex_t api_requests_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t api_responses_mutex = PTHREAD_MUTEX_INITIALIZER;
std::vector<APIRequest> api_request_queue;
std::vector<Packet> api_response_queue;

void disconnect() {
  // TODO @kiwec: send a logout packet first?
  // TODO @kiwec: don't set auth_header here, enqueue a disconnect packet
  // instead?

  pthread_mutex_lock(&outgoing_mutex);
  try_logging_in = false;
  auth_header = "";
  delete outgoing.memory;
  outgoing = {0};
  pthread_mutex_unlock(&outgoing_mutex);

  bancho.user_id = 0;
  bancho.osu->m_optionsMenu->logInButton->setText("Log in");
  bancho.osu->m_optionsMenu->logInButton->setColor(0xff00ff00);
  bancho.osu->m_optionsMenu->logInButton->is_loading = false;
}

void reconnect() {
  disconnect();

  // Disable autologin, in case there's an error while logging in
  // Will be reenabled after the login succeeds
  convar->getConVarByName("mp_autologin")->setValue(false);

  UString cv_password = convar->getConVarByName("mp_password")->getString();
  if(cv_password.length() == 0) {
    // No password: don't try to log in
    return;
  }

  const char* pw = cv_password.toUtf8(); // cv_password needs to stay in scope!
  bancho.pw_md5 = md5((uint8_t *)pw, strlen(pw));

  bancho.username = convar->getConVarByName("name")->getString();
  bancho.endpoint = convar->getConVarByName("mp_server")->getString();

  bancho.osu->m_optionsMenu->logInButton->is_loading = true;
  Packet new_login_packet = build_login_packet();

  pthread_mutex_lock(&outgoing_mutex);
  delete login_packet.memory;
  login_packet = new_login_packet;
  try_logging_in = true;
  pthread_mutex_unlock(&outgoing_mutex);
}

static size_t curl_write(void *contents, size_t size, size_t nmemb,
                         void *userp) {
  size_t realsize = size * nmemb;
  Packet *mem = (Packet *)userp;

  uint8_t *ptr = (uint8_t *)realloc(mem->memory, mem->size + realsize + 1);
  if (!ptr) {
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
  Packet response = {0};
  response.id = api_out.type;
  response.extra = api_out.extra;
  response.memory = new uint8_t[2048];

  auto query_url = UString::format("https://osu.%s%s", bancho.endpoint.toUtf8(), api_out.path.c_str());
  debugLog("Sending request: %s\n", query_url.toUtf8());

  curl_easy_setopt(curl, CURLOPT_URL, query_url.toUtf8());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);

  CURLcode res = curl_easy_perform(curl);
  if (res == CURLE_OK) {
    pthread_mutex_lock(&api_responses_mutex);
    api_response_queue.push_back(response);
    pthread_mutex_unlock(&api_responses_mutex);
  }

  curl_easy_reset(curl);
}

static void send_bancho_packet(CURL *curl, Packet outgoing) {
  Packet response = {0};
  response.memory = (uint8_t *)malloc(2048);

  struct curl_slist *chunk = NULL;
  if (!auth_header.empty()) {
    chunk = curl_slist_append(chunk, auth_header.c_str());
    chunk = curl_slist_append(chunk, "x-mcosu-ver: " MCOSU_VERSION);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);
  }

  auto query_url = UString::format("https://c.%s/", bancho.endpoint.toUtf8());
  curl_easy_setopt(curl, CURLOPT_URL, query_url.toUtf8());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, outgoing.memory);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, outgoing.pos);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "osu!");

  last_packet_tms = time(NULL);
  CURLcode res = curl_easy_perform(curl);
  CURLHcode hres;
  if (res != CURLE_OK) {
    // TODO @kiwec: if log in packet, display error to user
    debugLog("Failed to send packet, cURL error %d\n%s\n", res,
             query_url.toUtf8());
    goto end;
  }

  // Update auth token if applicable
  struct curl_header *header;
  hres = curl_easy_header(curl, "cho-token", 0, CURLH_HEADER, -1, &header);
  if (hres == CURLHE_OK) {
    auth_header = "osu-token: " + std::string(header->value);
  }
  hres = curl_easy_header(curl, "x-mcosu-features", 0, CURLH_HEADER, -1, &header);
  if (hres == CURLHE_OK) {
    bancho.submit_scores = strstr(header->value, "submit=1") != NULL;
  }

  while (response.pos < response.size) {
    uint16_t packet_id = read_short(&response);
    response.pos++;
    uint32_t packet_len = read_int(&response);
    if (packet_len > 10485760) {
      debugLog("Received a packet over 10Mb! Dropping response.\n");
      goto end;
    }

    Packet incoming = {
        .id = packet_id,
        .memory = new uint8_t[packet_len],
        .size = packet_len,
        .pos = 0,
    };
    memcpy(incoming.memory, response.memory + response.pos, packet_len);

    pthread_mutex_lock(&incoming_mutex);
    incoming_queue.push_back(incoming);
    pthread_mutex_unlock(&incoming_mutex);

    seconds_between_pings = 1.0;
    response.pos += packet_len;
  }

end:
  curl_easy_reset(curl);
  delete response.memory;
  curl_slist_free_all(chunk);
}

static void *do_networking(void *data) {
  (void)data;

  last_packet_tms = time(NULL);

  curl_global_init(CURL_GLOBAL_ALL);
  CURL *curl = curl_easy_init();
  if (!curl) {
    debugLog("Failed to initialize cURL, online functionality disabled.\n");
    return NULL;
  }

  while (true) {
    pthread_mutex_lock(&api_requests_mutex);
    if (api_request_queue.empty()) {
      pthread_mutex_unlock(&api_requests_mutex);
    } else {
      APIRequest api_out = api_request_queue.front();
      api_request_queue.erase(api_request_queue.begin());
      pthread_mutex_unlock(&api_requests_mutex);

      send_api_request(curl, api_out);
    }

    bool should_ping = difftime(time(NULL), last_packet_tms) > seconds_between_pings;
    if(bancho.user_id == 0) should_ping = false;
    // TODO @kiwec: if(is_in_lobby_list || is_multiplaying) should_ping = true;
    // needs proper mutex handling

    pthread_mutex_lock(&outgoing_mutex);
    if (try_logging_in) {
      Packet login = login_packet;
      login_packet = {0};
      try_logging_in = false;
      pthread_mutex_unlock(&outgoing_mutex);
      send_bancho_packet(curl, login);
      delete login.memory;
      pthread_mutex_lock(&outgoing_mutex);
    } else if(should_ping && outgoing.pos == 0) {
      write_short(&outgoing, PING);
      write_byte(&outgoing, 0);
      write_int(&outgoing, 0);

      // Polling gets slower over time, but resets when we receive new data
      if (seconds_between_pings < 30.0) {
        seconds_between_pings += 1.0;
      }
    }

    if(outgoing.pos > 0) {
      Packet out = outgoing;
      outgoing = {0};
      pthread_mutex_unlock(&outgoing_mutex);

      // DEBUG: If we're not sending the right amount of bytes, bancho.py just
      // chugs along! To try to detect it faster, we'll send two packets per request.
      write_short(&out, PING);
      write_byte(&out, 0);
      write_int(&out, 0);

      send_bancho_packet(curl, out);
      delete out.memory;
    } else {
      pthread_mutex_unlock(&outgoing_mutex);
    }

    // TODO @kiwec: this sucks. just because the ppy client does it that way,
    //   doesn't mean we also should... at least, we should send requests instantly
    //   if we haven't in the last second.

    // osu! doesn't need fast networking. In fact, we want to avoid spamming the
    // servers... So we batch requests and send them once a second at most.
    sleep(1);
  }

  // unreachable
  return NULL;
}
#endif

static void handle_api_response(Packet packet) {
  if (packet.id == GET_MAP_LEADERBOARD) {
    process_leaderboard_response(packet);
  } else if(packet.id == GET_BEATMAPSET_INFO) {
    OsuRoom::process_beatmapset_info_response(packet);
  } else {
    // NOTE: API Response type is same as API Request type
    debugLog("No handler for API response type %d!\n", packet.id);
  }
}

void receive_api_responses() {
#ifdef MCENGINE_FEATURE_PTHREADS
  pthread_mutex_lock(&api_responses_mutex);
  while (!api_response_queue.empty()) {
    Packet incoming = api_response_queue.front();
    api_response_queue.erase(api_response_queue.begin());
    handle_api_response(incoming);
    delete incoming.memory;
    delete incoming.extra;
  }
  pthread_mutex_unlock(&api_responses_mutex);
#endif
}

void receive_bancho_packets() {
#ifdef MCENGINE_FEATURE_PTHREADS
  pthread_mutex_lock(&incoming_mutex);
  while (!incoming_queue.empty()) {
    Packet incoming = incoming_queue.front();
    incoming_queue.erase(incoming_queue.begin());
    handle_packet(&incoming);
    delete incoming.memory;
  }
  pthread_mutex_unlock(&incoming_mutex);
#endif
}

void send_api_request(APIRequest request) {
  if (bancho.user_id <= 0) {
    debugLog("Cannot send API request of type %d since we are not logged in.\n",
             request.type);
    return;
  }

#ifdef MCENGINE_FEATURE_PTHREADS
  pthread_mutex_lock(&api_requests_mutex);

  // Jank way to do things... remove outdated requests now
  api_request_queue.erase(std::remove_if(api_request_queue.begin(), api_request_queue.end(), [request](APIRequest r) {
    return r.type = request.type;
  }), api_request_queue.end());

  api_request_queue.push_back(request);

  pthread_mutex_unlock(&api_requests_mutex);
#endif
}

void send_packet(Packet& packet) {
#ifdef MCENGINE_FEATURE_PTHREADS
  if(bancho.user_id <= 0) {
    // Don't queue any packets until we're logged in
    return;
  }

  // debugLog("Sending packet of type %hu: ", packet.id);
  // for (int i = 0; i < packet.pos; i++) {
  //     debugLog("%02x ", packet.memory[i]);
  // }
  // debugLog("\n");

  pthread_mutex_lock(&outgoing_mutex);

  // We're not sending it immediately, instead we just add it to the pile of
  // packets to send
  write_short(&outgoing, packet.id);
  write_byte(&outgoing, 0);
  write_int(&outgoing, packet.pos);
  write_bytes(&outgoing, packet.memory, packet.pos);

  pthread_mutex_unlock(&outgoing_mutex);
#else
  delete packet.memory;
#endif
}

void init_networking_thread() {
#ifdef MCENGINE_FEATURE_PTHREADS
  int ret = pthread_create(&networking_thread, NULL, do_networking, NULL);
  if (ret) {
    debugLog("Failed to start networking thread: pthread_create() returned "
             "code %i\n",
             ret);
  }
#endif
}
