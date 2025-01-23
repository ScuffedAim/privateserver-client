#include "Chat.h"

#include <regex>

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "BanchoUsers.h"
#include "Beatmap.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "CBaseUITextbox.h"
#include "ChatLink.h"
#include "Engine.h"
#include "Font.h"
#include "Keyboard.h"
#include "Lobby.h"
#include "ModSelector.h"
#include "OptionsMenu.h"
#include "Osu.h"
#include "PauseMenu.h"
#include "PromptScreen.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "RoomScreen.h"
#include "Skin.h"
#include "SongBrowser/ScoreButton.h"
#include "SongBrowser/SongBrowser.h"
#include "SoundEngine.h"
#include "SpectatorScreen.h"
#include "UIButton.h"
#include "UIUserContextMenu.h"

ChatChannel::ChatChannel(Chat *chat, UString name_arg) {
    this->chat = chat;
    this->name = name_arg;

    this->ui = new CBaseUIScrollView(0, 0, 0, 0, "");
    this->ui->setDrawFrame(false);
    this->ui->setDrawBackground(true);
    this->ui->setBackgroundColor(0xdd000000);
    this->ui->setHorizontalScrolling(false);
    this->ui->setDrawScrollbars(true);
    this->ui->sticky = true;

    this->btn = new UIButton(0, 0, 0, 0, "button", this->name);
    this->btn->grabs_clicks = true;
    this->btn->setUseDefaultSkin();
    this->btn->setClickCallback(fastdelegate::MakeDelegate(this, &ChatChannel::onChannelButtonClick));
    this->chat->button_container->addBaseUIElement(this->btn);
}

ChatChannel::~ChatChannel() {
    delete this->ui;
    this->chat->button_container->deleteBaseUIElement(this->btn);
}

void ChatChannel::onChannelButtonClick(CBaseUIButton *btn) {
    engine->getSound()->play(osu->getSkin()->clickButton);
    this->chat->switchToChannel(this);
}

void ChatChannel::add_message(ChatMessage msg) {
    const float line_height = 20;
    const Color system_color = 0xffffff00;

    float x = 10;

    bool is_action = msg.text.startsWith("\001ACTION");
    if(is_action) {
        msg.text.erase(0, 7);
        if(msg.text.endsWith("\001")) {
            msg.text.erase(msg.text.length() - 1, 1);
        }
    }

    struct tm *tm = localtime(&msg.tms);
    auto timestamp_str = UString::format("%02d:%02d ", tm->tm_hour, tm->tm_min);
    if(is_action) timestamp_str.append("*");
    float time_width = this->chat->font->getStringWidth(timestamp_str);
    CBaseUILabel *timestamp = new CBaseUILabel(x, this->y_total, time_width, line_height, "", timestamp_str);
    timestamp->setDrawFrame(false);
    timestamp->setDrawBackground(false);
    this->ui->getContainer()->addBaseUIElement(timestamp);
    x += time_width;

    bool is_system_message = msg.author_name.length() == 0;
    if(!is_system_message) {
        float name_width = this->chat->font->getStringWidth(msg.author_name);
        auto user_box = new UIUserLabel(msg.author_id, msg.author_name);
        user_box->setTextColor(0xff2596be);
        user_box->setPos(x, this->y_total);
        user_box->setSize(name_width, line_height);
        this->ui->getContainer()->addBaseUIElement(user_box);
        x += name_width;

        if(!is_action) {
            msg.text.insert(0, ": ");
        }
    }

    // regex101 format: (\[\[(.+?)\]\])|(\[((\S+):\/\/\S+) (.+?)\])|(https?:\/\/\S+)
    // This matches:
    // - Raw URLs      https://example.com
    // - Labeled URLs  [https://regex101.com useful website]
    // - Lobby invites [osump://0/ join my lobby plz]
    // - Wiki links    [[Chat Console]]
    //
    // Group 1) [[FAQ]]
    // Group 2) FAQ
    // Group 3) [https://regex101.com label]
    // Group 4) https://regex101.com
    // Group 5) https
    // Group 6) label
    // Group 7) https://example.com
    //
    // Groups 1, 2 only exist for wiki links
    // Groups 3, 4, 5, 6 only exist for labeled links
    // Group 7 only exists for raw links
    std::wregex url_regex(L"(\\[\\[(.+?)\\]\\])|(\\[((\\S+)://\\S+) (.+?)\\])|(https?://\\S+)");

    std::wstring msg_text = msg.text.wc_str();
    std::wsmatch match;
    std::vector<CBaseUILabel *> text_fragments;
    std::wstring::const_iterator search_start = msg_text.cbegin();
    int text_idx = 0;
    while(std::regex_search(search_start, msg_text.cend(), match, url_regex)) {
        int match_pos;
        int match_len;
        UString link_url;
        UString link_label;
        if(match[7].matched) {
            // Raw link
            match_pos = match.position(7);
            match_len = match.length(7);
            link_url = match.str(7).c_str();
            link_label = match.str(7).c_str();
        } else if(match[3].matched) {
            // Labeled link
            match_pos = match.position(3);
            match_len = match.length(3);
            link_url = match.str(4).c_str();
            link_label = match.str(6).c_str();

            // Normalize invite links to osump://
            UString link_protocol = match.str(5).c_str();
            if(link_protocol == UString("osu")) {
                // osu:// -> osump://
                link_url.insert(2, "mp");
            } else if(link_protocol == UString("http://osump")) {
                // http://osump:// -> osump://
                link_url.erase(0, 7);
            }
        } else {
            // Wiki link
            match_pos = match.position(1);
            match_len = match.length(1);
            link_url = "https://osu.ppy.sh/wiki/";
            link_url.append(match.str(2).c_str());
            link_label = "wiki:";
            link_label.append(match.str(2).c_str());
        }

        int search_idx = search_start - msg_text.cbegin();
        auto url_start = search_idx + match_pos;
        auto preceding_text = msg.text.substr(text_idx, url_start - text_idx);
        if(preceding_text.length() > 0) {
            text_fragments.push_back(new CBaseUILabel(0, 0, 0, 0, "", preceding_text));
        }

        auto link = new ChatLink(0, 0, 0, 0, link_url, link_label);
        text_fragments.push_back(link);

        text_idx = url_start + match_len;
        search_start = msg_text.cbegin() + text_idx;
    }
    if(is_action) {
        // Only appending now to prevent this character from being included in a link
        msg.text.append(L"*");
        msg_text.append(L"*");
    }
    if(search_start != msg_text.cend()) {
        auto text = msg.text.substr(text_idx);
        text_fragments.push_back(new CBaseUILabel(0, 0, 0, 0, "", text));
    }

    // We're offsetting the first fragment to account for the username + timestamp
    // Since first fragment will always be text, we don't care about the size being wrong
    float line_width = x;

    // We got a bunch of text fragments, now position them, and if we start a new line,
    // possibly divide them into more text fragments.
    for(auto fragment : text_fragments) {
        UString text_str("");
        auto fragment_text = fragment->getText();

        for(int i = 0; i < fragment_text.length(); i++) {
            float char_width = this->chat->font->getGlyphMetrics(fragment_text[i]).advance_x;
            if(line_width + char_width + 20 >= this->chat->getSize().x) {
                ChatLink *link_fragment = dynamic_cast<ChatLink *>(fragment);
                if(link_fragment == NULL) {
                    CBaseUILabel *text = new CBaseUILabel(x, this->y_total, line_width - x, line_height, "", text_str);
                    text->setDrawFrame(false);
                    text->setDrawBackground(false);
                    if(is_system_message) {
                        text->setTextColor(system_color);
                    }
                    this->ui->getContainer()->addBaseUIElement(text);
                } else {
                    ChatLink *link =
                        new ChatLink(x, this->y_total, line_width - x, line_height, fragment->getName(), text_str);
                    this->ui->getContainer()->addBaseUIElement(link);
                }

                x = 10;
                this->y_total += line_height;
                line_width = x;
                text_str = "";
            }

            text_str.append(fragment_text[i]);
            line_width += char_width;
        }

        ChatLink *link_fragment = dynamic_cast<ChatLink *>(fragment);
        if(link_fragment == NULL) {
            CBaseUILabel *text = new CBaseUILabel(x, this->y_total, line_width - x, line_height, "", text_str);
            text->setDrawFrame(false);
            text->setDrawBackground(false);
            if(is_system_message) {
                text->setTextColor(system_color);
            }
            this->ui->getContainer()->addBaseUIElement(text);
        } else {
            ChatLink *link = new ChatLink(x, this->y_total, line_width - x, line_height, fragment->getName(), text_str);
            this->ui->getContainer()->addBaseUIElement(link);
        }

        x = line_width;
    }

    this->y_total += line_height;
    this->ui->setScrollSizeToContent();
}

void ChatChannel::updateLayout(Vector2 pos, Vector2 size) {
    this->ui->clear();
    this->ui->setPos(pos);
    this->ui->setSize(size);
    this->y_total = 7;

    for(auto msg : this->messages) {
        this->add_message(msg);
    }

    this->ui->setScrollSizeToContent();
}

Chat::Chat() : OsuScreen() {
    this->font = engine->getResourceManager()->getFont("FONT_DEFAULT");

    this->button_container = new CBaseUIContainer(0, 0, 0, 0, "");

    this->join_channel_btn = new UIButton(0, 0, 0, 0, "button", "+");
    this->join_channel_btn->setUseDefaultSkin();
    this->join_channel_btn->setColor(0xffffff55);
    this->join_channel_btn->setSize(this->button_height + 2, this->button_height + 2);
    this->join_channel_btn->setClickCallback(fastdelegate::MakeDelegate(this, &Chat::askWhatChannelToJoin));
    this->button_container->addBaseUIElement(this->join_channel_btn);

    this->input_box = new CBaseUITextbox(0, 0, 0, 0, "");
    this->input_box->setDrawFrame(false);
    this->input_box->setDrawBackground(true);
    this->input_box->setBackgroundColor(0xdd000000);
    this->addBaseUIElement(this->input_box);

    this->updateLayout(osu->getScreenSize());
}

Chat::~Chat() { delete this->button_container; }

void Chat::draw(Graphics *g) {
    const bool isAnimating = anim->isAnimating(&this->fAnimation);
    if(!this->bVisible && !isAnimating) return;

    if(isAnimating) {
        // XXX: Setting BLEND_MODE_PREMUL_ALPHA is not enough, transparency is still incorrect
        osu->getSliderFrameBuffer()->enable();
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
    }

    if(this->selected_channel == NULL) {
        const float chat_h = round(this->getSize().y * 0.3f);
        const float chat_y = this->getSize().y - chat_h;
        float chat_w = this->getSize().x;
        if(this->isSmallChat()) {
            // In the lobby and in multi rooms, don't take the full horizontal width to allow for cleaner UI designs.
            chat_w = round(chat_w * 0.6);
        }

        g->setColor(COLOR(100, 0, 10, 50));
        g->fillRect(0, chat_y - (this->button_height + 2.f), chat_w, (this->button_height + 2.f));
        g->setColor(COLOR(150, 0, 0, 0));
        g->fillRect(0, chat_y, chat_w, chat_h);
    } else {
        g->setColor(COLOR(100, 0, 10, 50));
        g->fillRect(this->button_container->getPos().x, this->button_container->getPos().y,
                    this->button_container->getSize().x, this->button_container->getSize().y);
        this->button_container->draw(g);

        OsuScreen::draw(g);
        if(this->selected_channel != NULL) {
            this->selected_channel->ui->draw(g);
        }
    }

    if(isAnimating) {
        osu->getSliderFrameBuffer()->disable();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
        g->push3DScene(McRect(0, 0, this->getSize().x, this->getSize().y));
        {
            g->rotate3DScene(-(1.0f - this->fAnimation) * 90, 0, 0);
            g->translate3DScene(0, -(1.0f - this->fAnimation) * this->getSize().y * 1.25f,
                                -(1.0f - this->fAnimation) * 700);

            osu->getSliderFrameBuffer()->setColor(COLORf(this->fAnimation, 1.0f, 1.0f, 1.0f));
            osu->getSliderFrameBuffer()->draw(g, 0, 0);
        }
        g->pop3DScene();
    }
}

void Chat::mouse_update(bool *propagate_clicks) {
    if(!this->bVisible) return;
    OsuScreen::mouse_update(propagate_clicks);
    this->button_container->mouse_update(propagate_clicks);
    if(this->selected_channel) {
        this->selected_channel->ui->mouse_update(propagate_clicks);
    }

    // Focus without placing the cursor at the end of the field
    this->input_box->focus(false);
}

void Chat::handle_command(UString msg) {
    if(msg == UString("/clear")) {
        this->selected_channel->messages.clear();
        this->updateLayout(osu->getScreenSize());
        return;
    }

    if(msg == UString("/close") || msg == UString("/p") || msg == UString("/part")) {
        this->leave(this->selected_channel->name);
        return;
    }

    if(msg == UString("/help") || msg == UString("/keys")) {
        env->openURLInDefaultBrowser("https://osu.ppy.sh/wiki/en/Client/Interface/Chat_console");
        return;
    }

    if(msg == UString("/np")) {
        auto diff = osu->getSelectedBeatmap()->getSelectedDifficulty2();
        if(diff == NULL) {
            this->addSystemMessage("You are not listening to anything.");
            return;
        }

        UString song_name = UString::format("%s - %s [%s]", diff->getArtist().c_str(), diff->getTitle().c_str(),
                                            diff->getDifficultyName().c_str());
        UString song_link = UString::format("[https://osu.%s/beatmaps/%d %s]", bancho.endpoint.toUtf8(), diff->getID(),
                                            song_name.toUtf8());

        UString np_msg;
        if(osu->isInPlayMode()) {
            np_msg = UString::format("\001ACTION is playing %s", song_link.toUtf8());

            auto mods = osu->getScore()->mods;
            if(mods.speed != 1.f) {
                auto speed_modifier = UString::format(" x%.1f", mods.speed);
                np_msg.append(speed_modifier);
            }
            auto mod_string = ScoreButton::getModsStringForDisplay(mods);
            if(mod_string.length() > 0) {
                np_msg.append(" (+");
                np_msg.append(mod_string);
                np_msg.append(")");
            }

            np_msg.append("\001");
        } else {
            np_msg = UString::format("\001ACTION is listening to %s\001", song_link.toUtf8());
        }

        this->send_message(np_msg);
        return;
    }

    if(msg.startsWith("/addfriend ")) {
        auto friend_name = msg.substr(11);
        auto user = find_user(friend_name);
        if(!user) {
            this->addSystemMessage(UString::format("User '%s' not found. Are they online?", friend_name.toUtf8()));
            return;
        }

        if(user->is_friend()) {
            this->addSystemMessage(UString::format("You are already friends with %s!", friend_name.toUtf8()));
        } else {
            Packet packet;
            packet.id = FRIEND_ADD;
            write<u32>(&packet, user->user_id);
            send_packet(packet);

            friends.push_back(user->user_id);

            this->addSystemMessage(UString::format("You are now friends with %s.", friend_name.toUtf8()));
        }

        return;
    }

    if(msg.startsWith("/bb ")) {
        this->addChannel("BanchoBot", true);
        this->send_message(msg.substr(4));
        return;
    }

    if(msg == UString("/away")) {
        this->away_msg = "";
        this->addSystemMessage("Away message removed.");
        return;
    }
    if(msg.startsWith("/away ")) {
        this->away_msg = msg.substr(6);
        this->addSystemMessage(UString::format("Away message set to '%s'.", this->away_msg.toUtf8()));
        return;
    }

    if(msg.startsWith("/delfriend ")) {
        auto friend_name = msg.substr(11);
        auto user = find_user(friend_name);
        if(!user) {
            this->addSystemMessage(UString::format("User '%s' not found. Are they online?", friend_name.toUtf8()));
            return;
        }

        if(user->is_friend()) {
            Packet packet;
            packet.id = FRIEND_REMOVE;
            write<u32>(&packet, user->user_id);
            send_packet(packet);

            auto it = std::find(friends.begin(), friends.end(), user->user_id);
            if(it != friends.end()) {
                friends.erase(it);
            }

            this->addSystemMessage(UString::format("You are no longer friends with %s.", friend_name.toUtf8()));
        } else {
            this->addSystemMessage(UString::format("You aren't friends with %s!", friend_name.toUtf8()));
        }

        return;
    }

    if(msg.startsWith("/me ")) {
        auto new_text = msg.substr(3);
        new_text.insert(0, "\001ACTION");
        new_text.append("\001");
        this->send_message(new_text);
        return;
    }

    if(msg.startsWith("/chat ") || msg.startsWith("/msg ") || msg.startsWith("/query ")) {
        auto username = msg.substr(msg.find(L" "));
        this->addChannel(username, true);
        return;
    }

    if(msg.startsWith("/invite ")) {
        if(!bancho.is_in_a_multi_room()) {
            this->addSystemMessage("You are not in a multiplayer room!");
            return;
        }

        auto username = msg.substr(8);
        auto invite_msg = UString::format("\001ACTION has invited you to join [osump://%d/%s %s]\001", bancho.room.id,
                                          bancho.room.password.toUtf8(), bancho.room.name.toUtf8());

        Packet packet;
        packet.id = SEND_PRIVATE_MESSAGE;
        write_string(&packet, (char *)bancho.username.toUtf8());
        write_string(&packet, (char *)invite_msg.toUtf8());
        write_string(&packet, (char *)username.toUtf8());
        write<u32>(&packet, bancho.user_id);
        send_packet(packet);

        this->addSystemMessage(UString::format("%s has been invited to the game.", username.toUtf8()));
        return;
    }

    if(msg.startsWith("/j ") || msg.startsWith("/join ")) {
        auto channel = msg.substr(msg.find(L" "));
        this->join(channel);
        return;
    }

    if(msg.startsWith("/p ") || msg.startsWith("/part ")) {
        auto channel = msg.substr(msg.find(L" "));
        this->leave(channel);
        return;
    }

    this->addSystemMessage("This command is not supported.");
}

void Chat::onKeyDown(KeyboardEvent &key) {
    if(!this->bVisible) return;

    if(engine->getKeyboard()->isAltDown()) {
        i32 tab_select = -1;
        if(key.getKeyCode() == KEY_1) tab_select = 0;
        if(key.getKeyCode() == KEY_2) tab_select = 1;
        if(key.getKeyCode() == KEY_3) tab_select = 2;
        if(key.getKeyCode() == KEY_4) tab_select = 3;
        if(key.getKeyCode() == KEY_5) tab_select = 4;
        if(key.getKeyCode() == KEY_6) tab_select = 5;
        if(key.getKeyCode() == KEY_7) tab_select = 6;
        if(key.getKeyCode() == KEY_8) tab_select = 7;
        if(key.getKeyCode() == KEY_9) tab_select = 8;
        if(key.getKeyCode() == KEY_0) tab_select = 9;

        if(tab_select != -1) {
            if(tab_select >= this->channels.size()) {
                key.consume();
                return;
            }

            key.consume();
            this->switchToChannel(this->channels[tab_select]);
            return;
        }
    }

    if(key.getKeyCode() == KEY_PAGEUP) {
        if(this->selected_channel != NULL) {
            key.consume();
            this->selected_channel->ui->scrollY(this->getSize().y - this->input_box_height);
            return;
        }
    }

    if(key.getKeyCode() == KEY_PAGEDOWN) {
        if(this->selected_channel != NULL) {
            key.consume();
            this->selected_channel->ui->scrollY(-(this->getSize().y - this->input_box_height));
            return;
        }
    }

    // Escape: close chat
    if(key.getKeyCode() == KEY_ESCAPE) {
        if(this->isVisibilityForced()) return;

        key.consume();
        this->user_wants_chat = false;
        this->updateVisibility();
        return;
    }

    // Return: send message
    if(key.getKeyCode() == KEY_ENTER) {
        key.consume();
        if(this->selected_channel != NULL && this->input_box->getText().length() > 0) {
            if(this->input_box->getText()[0] == L'/') {
                this->handle_command(this->input_box->getText());
            } else {
                this->send_message(this->input_box->getText());
            }

            engine->getSound()->play(osu->getSkin()->messageSent);
            this->input_box->clear();
        }
        this->tab_completion_prefix = "";
        this->tab_completion_match = "";
        return;
    }

    // Ctrl+W: Close current channel
    if(engine->getKeyboard()->isControlDown() && key.getKeyCode() == KEY_W) {
        key.consume();
        if(this->selected_channel != NULL) {
            this->leave(this->selected_channel->name);
        }
        return;
    }

    // Ctrl+Tab: Switch channels
    // KEY_TAB doesn't work on Linux
    if(engine->getKeyboard()->isControlDown() && (key.getKeyCode() == 65056 || key.getKeyCode() == KEY_TAB)) {
        key.consume();
        if(this->selected_channel == NULL) return;
        int chan_index = this->channels.size();
        for(auto chan : this->channels) {
            if(chan == this->selected_channel) {
                break;
            }
            chan_index++;
        }

        if(engine->getKeyboard()->isShiftDown()) {
            // Ctrl+Shift+Tab: Go to previous channel
            auto new_chan = this->channels[(chan_index - 1) % this->channels.size()];
            this->switchToChannel(new_chan);
        } else {
            // Ctrl+Tab: Go to next channel
            auto new_chan = this->channels[(chan_index + 1) % this->channels.size()];
            this->switchToChannel(new_chan);
        }

        engine->getSound()->play(osu->getSkin()->clickButton);

        return;
    }

    // TAB: Complete nickname
    // KEY_TAB doesn't work on Linux
    if(key.getKeyCode() == 65056 || key.getKeyCode() == KEY_TAB) {
        key.consume();

        auto text = this->input_box->getText();
        i32 username_start_idx = text.findLast(" ", 0, this->input_box->iCaretPosition) + 1;
        i32 username_end_idx = this->input_box->iCaretPosition;
        i32 username_len = username_end_idx - username_start_idx;

        if(this->tab_completion_prefix.length() == 0) {
            this->tab_completion_prefix = text.substr(username_start_idx, username_len);
        } else {
            username_start_idx = this->input_box->iCaretPosition - this->tab_completion_match.length();
            username_len = username_end_idx - username_start_idx;
        }

        auto user = find_user_starting_with(this->tab_completion_prefix, this->tab_completion_match);
        if(user) {
            this->tab_completion_match = user->name;

            // Remove current username, add new username
            this->input_box->sText.erase(this->input_box->iCaretPosition - username_len, username_len);
            this->input_box->iCaretPosition -= username_len;
            this->input_box->sText.insert(this->input_box->iCaretPosition, this->tab_completion_match);
            this->input_box->iCaretPosition += this->tab_completion_match.length();
            this->input_box->setText(this->input_box->sText);
            this->input_box->updateTextPos();
            this->input_box->tickCaret();

            Sound *sounds[] = {osu->getSkin()->typing1, osu->getSkin()->typing2, osu->getSkin()->typing3,
                               osu->getSkin()->typing4};
            engine->getSound()->play(sounds[rand() % 4]);
        }

        return;
    }

    // Typing in chat: capture keypresses
    if(!engine->getKeyboard()->isAltDown()) {
        this->tab_completion_prefix = "";
        this->tab_completion_match = "";
        this->input_box->onKeyDown(key);
        key.consume();
        return;
    }
}

void Chat::onKeyUp(KeyboardEvent &key) {
    if(!this->bVisible || key.isConsumed()) return;

    this->input_box->onKeyUp(key);
    key.consume();
}

void Chat::onChar(KeyboardEvent &key) {
    if(!this->bVisible || key.isConsumed()) return;

    this->input_box->onChar(key);
    key.consume();
}

void Chat::mark_as_read(ChatChannel *chan) {
    if(!this->bVisible) return;

    // XXX: Only mark as read after 500ms
    chan->read = true;

    CURL *curl = curl_easy_init();
    if(!curl) {
        debugLog("Failed to initialize cURL in Chat::mark_as_read()!\n");
        return;
    }
    char *channel_urlencoded = curl_easy_escape(curl, chan->name.toUtf8(), 0);
    if(!channel_urlencoded) {
        debugLog("Failed to encode channel name!\n");
        curl_easy_cleanup(curl);
        return;
    }

    APIRequest request;
    request.type = MARK_AS_READ;
    request.path = UString::format("/web/osu-markasread.php?u=%s&h=%s&channel=%s", bancho.username.toUtf8(),
                                   bancho.pw_md5.toUtf8(), channel_urlencoded);
    request.mime = NULL;

    send_api_request(request);

    curl_free(channel_urlencoded);
    curl_easy_cleanup(curl);
}

void Chat::switchToChannel(ChatChannel *chan) {
    this->selected_channel = chan;
    if(!chan->read) {
        this->mark_as_read(this->selected_channel);
    }

    // Update button colors
    this->updateButtonLayout(this->getSize());
}

void Chat::addChannel(UString channel_name, bool switch_to) {
    for(auto chan : this->channels) {
        if(chan->name == channel_name) {
            if(switch_to) {
                this->switchToChannel(chan);
            }
            return;
        }
    }

    ChatChannel *chan = new ChatChannel(this, channel_name);
    this->channels.push_back(chan);

    if(this->selected_channel == NULL && this->channels.size() == 1) {
        this->switchToChannel(chan);
    } else if(channel_name == UString("#multiplayer") || channel_name == UString("#lobby")) {
        this->switchToChannel(chan);
    } else if(switch_to) {
        this->switchToChannel(chan);
    }

    this->updateLayout(osu->getScreenSize());

    if(this->isVisible()) {
        engine->getSound()->play(osu->getSkin()->expand);
    }
}

void Chat::addMessage(UString channel_name, ChatMessage msg, bool mark_unread) {
    bool is_pm = msg.author_id > 0 && channel_name[0] != '#' && msg.author_name != bancho.username;
    if(is_pm) {
        // If it's a PM, the channel title should be the one who sent the message
        channel_name = msg.author_name;
    }

    this->addChannel(channel_name);
    for(auto chan : this->channels) {
        if(chan->name != channel_name) continue;
        chan->messages.push_back(msg);
        chan->add_message(msg);

        if(mark_unread) {
            chan->read = false;
            if(chan == this->selected_channel) {
                this->mark_as_read(chan);
            } else {
                this->updateButtonLayout(this->getSize());
            }
        }

        if(chan->messages.size() > 100) {
            chan->messages.erase(chan->messages.begin());
        }

        break;
    }

    if(is_pm && this->away_msg.length() > 0) {
        Packet packet;
        packet.id = SEND_PRIVATE_MESSAGE;
        write_string(&packet, (char *)bancho.username.toUtf8());
        write_string(&packet, (char *)this->away_msg.toUtf8());
        write_string(&packet, (char *)msg.author_name.toUtf8());
        write<u32>(&packet, bancho.user_id);
        send_packet(packet);

        // Server doesn't echo the message back
        this->addMessage(channel_name, ChatMessage{
                                           .tms = time(NULL),
                                           .author_id = bancho.user_id,
                                           .author_name = bancho.username,
                                           .text = this->away_msg,
                                       });
    }
}

void Chat::addSystemMessage(UString msg) {
    this->addMessage(this->selected_channel->name, ChatMessage{
                                                       .tms = time(NULL),
                                                       .author_id = 0,
                                                       .author_name = "",
                                                       .text = msg,
                                                   });
}

void Chat::removeChannel(UString channel_name) {
    ChatChannel *chan = NULL;
    for(auto c : this->channels) {
        if(c->name == channel_name) {
            chan = c;
            break;
        }
    }

    if(chan == NULL) return;

    auto it = std::find(this->channels.begin(), this->channels.end(), chan);
    this->channels.erase(it);
    if(this->selected_channel == chan) {
        this->selected_channel = NULL;
        if(!this->channels.empty()) {
            this->switchToChannel(this->channels[0]);
        }
    }

    delete chan;
    this->updateButtonLayout(this->getSize());
}

void Chat::updateLayout(Vector2 newResolution) {
    // We don't want to update while the chat is hidden, to avoid lagspikes during gameplay
    if(!this->bVisible) {
        this->layout_update_scheduled = true;
        return;
    }

    // In the lobby and in multi rooms don't take the full horizontal width to allow for cleaner UI designs.
    if(this->isSmallChat()) {
        newResolution.x = round(newResolution.x * 0.6);
    }

    this->setSize(newResolution);

    const float chat_w = newResolution.x;
    const float chat_h = round(newResolution.y * 0.3f) - this->input_box_height;
    const float chat_y = newResolution.y - (chat_h + this->input_box_height);
    for(auto chan : this->channels) {
        chan->updateLayout(Vector2{0.f, chat_y}, Vector2{chat_w, chat_h});
    }

    this->input_box->setPos(Vector2{0.f, chat_y + chat_h});
    this->input_box->setSize(Vector2{chat_w, this->input_box_height});

    if(this->selected_channel == NULL && !this->channels.empty()) {
        this->selected_channel = this->channels[0];
        this->selected_channel->read = true;
    }

    this->updateButtonLayout(newResolution);
    this->updateButtonLayout(newResolution);  // idk
    this->layout_update_scheduled = false;
}

void Chat::updateButtonLayout(Vector2 screen) {
    const float initial_x = 2;
    float total_x = initial_x;

    std::sort(this->channels.begin(), this->channels.end(),
              [](ChatChannel *a, ChatChannel *b) { return a->name < b->name; });

    // Look, I really tried. But for some reason setPos() doesn't work until we change
    // the screen resolution once. So I'll just compute absolute position manually.
    float button_container_height = this->button_height + 2;
    for(auto chan : this->channels) {
        UIButton *btn = chan->btn;
        float button_width = this->font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            button_container_height += this->button_height;
        }

        total_x += button_width;
    }

    const float chat_y = round(screen.y * 0.7f);
    float total_y = 0.f;
    total_x = initial_x;
    for(auto chan : this->channels) {
        UIButton *btn = chan->btn;
        float button_width = this->font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            total_y += this->button_height;
        }

        btn->setPos(total_x, chat_y - button_container_height + total_y);
        // Buttons are drawn a bit smaller than they should, so we up the size here
        btn->setSize(button_width + 2, this->button_height + 2);

        if(this->selected_channel->name == btn->getText()) {
            btn->setColor(0xfffefffd);
        } else {
            if(chan->read) {
                btn->setColor(0xff38439f);
            } else {
                btn->setColor(0xff88a0f7);
            }
        }

        total_x += button_width;
    }

    this->join_channel_btn->setPos(total_x, chat_y - button_container_height + total_y);
    this->button_container->setPos(0, chat_y - button_container_height);
    this->button_container->setSize(screen.x, button_container_height);
}

void Chat::join(UString channel_name) {
    // XXX: Open the channel immediately, without letting the user send messages in it.
    //      Would require a way to signal if a channel is joined or not.
    //      Would allow to keep open the tabs of the channels we got kicked out of.
    Packet packet;
    packet.id = CHANNEL_JOIN;
    write_string(&packet, channel_name.toUtf8());
    send_packet(packet);
}

void Chat::leave(UString channel_name) {
    bool send_leave_packet = channel_name[0] == '#';
    if(channel_name == UString("#lobby")) send_leave_packet = false;
    if(channel_name == UString("#multiplayer")) send_leave_packet = false;

    if(send_leave_packet) {
        Packet packet;
        packet.id = CHANNEL_PART;
        write_string(&packet, channel_name.toUtf8());
        send_packet(packet);
    }

    this->removeChannel(channel_name);

    engine->getSound()->play(osu->getSkin()->closeChatTab);
}

void Chat::send_message(UString msg) {
    Packet packet;
    packet.id = this->selected_channel->name[0] == '#' ? SEND_PUBLIC_MESSAGE : SEND_PRIVATE_MESSAGE;
    write_string(&packet, (char *)bancho.username.toUtf8());
    write_string(&packet, (char *)msg.toUtf8());
    write_string(&packet, (char *)this->selected_channel->name.toUtf8());
    write<u32>(&packet, bancho.user_id);
    send_packet(packet);

    // Server doesn't echo the message back
    this->addMessage(this->selected_channel->name, ChatMessage{
                                                       .tms = time(NULL),
                                                       .author_id = bancho.user_id,
                                                       .author_name = bancho.username,
                                                       .text = msg,
                                                   });
}

void Chat::onDisconnect() {
    for(auto chan : this->channels) {
        delete chan;
    }
    this->channels.clear();

    for(auto chan : chat_channels) {
        delete chan.second;
    }
    chat_channels.clear();

    this->selected_channel = NULL;
    this->updateLayout(osu->getScreenSize());

    this->updateVisibility();
}

void Chat::onResolutionChange(Vector2 newResolution) { this->updateLayout(newResolution); }

bool Chat::isSmallChat() {
    if(osu->room == NULL || osu->lobby == NULL || osu->songBrowser2 == NULL) return false;
    bool sitting_in_room =
        osu->room->isVisible() && !osu->songBrowser2->isVisible() && !bancho.is_playing_a_multi_map();
    bool sitting_in_lobby = osu->lobby->isVisible();
    return sitting_in_room || sitting_in_lobby;
}

bool Chat::isVisibilityForced() {
    bool is_forced = (this->isSmallChat() || osu->spectatorScreen->isVisible());
    if(is_forced != this->visibility_was_forced) {
        // Chat width changed: update the layout now
        this->visibility_was_forced = is_forced;
        this->updateLayout(osu->getScreenSize());
    }
    return is_forced;
}

void Chat::updateVisibility() {
    auto selected_beatmap = osu->getSelectedBeatmap();
    bool can_skip = (selected_beatmap != NULL) && (selected_beatmap->isInSkippableSection());
    bool is_spectating = cv_mod_autoplay.getBool() || (cv_mod_autopilot.getBool() && cv_mod_relax.getBool()) ||
                         (selected_beatmap != NULL && selected_beatmap->is_watching) || bancho.spectated_player_id != 0;
    bool is_clicking_circles = osu->isInPlayMode() && !can_skip && !is_spectating && !osu->pauseMenu->isVisible();
    if(bancho.is_playing_a_multi_map() && !bancho.room.all_players_loaded) {
        is_clicking_circles = false;
    }
    bool force_hide = osu->optionsMenu->isVisible() || osu->modSelector->isVisible() || is_clicking_circles;
    if(!bancho.is_online()) force_hide = true;

    if(force_hide) {
        this->setVisible(false);
    } else if(this->isVisibilityForced()) {
        this->setVisible(true);
    } else {
        this->setVisible(this->user_wants_chat);
    }
}

CBaseUIContainer *Chat::setVisible(bool visible) {
    if(visible == this->bVisible) return this;

    engine->getSound()->play(osu->getSkin()->clickButton);

    if(visible && bancho.user_id <= 0) {
        osu->optionsMenu->askForLoginDetails();
        return this;
    }

    this->bVisible = visible;
    if(visible) {
        osu->optionsMenu->setVisible(false);
        anim->moveQuartOut(&this->fAnimation, 1.0f, 0.25f * (1.0f - this->fAnimation), true);

        if(this->selected_channel != NULL && !this->selected_channel->read) {
            this->mark_as_read(this->selected_channel);
        }

        if(this->layout_update_scheduled) {
            this->updateLayout(osu->getScreenSize());
        }
    } else {
        anim->moveQuadOut(&this->fAnimation, 0.0f, 0.25f * this->fAnimation, true);
    }

    return this;
}

bool Chat::isMouseInChat() {
    if(!this->isVisible()) return false;
    if(this->selected_channel == NULL) return false;
    return this->input_box->isMouseInside() || this->selected_channel->ui->isMouseInside();
}

void Chat::askWhatChannelToJoin(CBaseUIButton *btn) {
    // XXX: Could display nicer UI with full channel list (chat_channels in Bancho.cpp)
    osu->prompt->prompt("Type in the channel you want to join (e.g. '#osu'):",
                        fastdelegate::MakeDelegate(this, &Chat::join));
}
