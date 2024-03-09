#include "OsuChat.h"

#include "AnimationHandler.h"
#include "Bancho.h"
#include "BanchoNetworking.h"
#include "CBaseUIButton.h"
#include "CBaseUIContainer.h"
#include "CBaseUILabel.h"
#include "Engine.h"
#include "Font.h"
#include "Keyboard.h"
#include "Osu.h"
#include "OsuBeatmap.h"
#include "OsuLobby.h"
#include "OsuModSelector.h"
#include "OsuOptionsMenu.h"
#include "OsuPauseMenu.h"
#include "OsuPromptScreen.h"
#include "OsuRoom.h"
#include "OsuSkin.h"
#include "OsuSongBrowser2.h"
#include "OsuUIButton.h"
#include "OsuUIUserContextMenu.h"
#include "RenderTarget.h"
#include "ResourceManager.h"
#include "SoundEngine.h"

OsuChatChannel::OsuChatChannel(OsuChat *chat, UString name_arg) {
    m_chat = chat;
    name = name_arg;

    ui = new CBaseUIScrollView(0, 0, 0, 0, "");
    ui->setDrawFrame(false);
    ui->setDrawBackground(true);
    ui->setBackgroundColor(0xdd000000);
    ui->setHorizontalScrolling(false);
    ui->setDrawScrollbars(true);
    ui->sticky = true;

    btn = new OsuUIButton(bancho.osu, 0, 0, 0, 0, "button", name);
    btn->grabs_clicks = true;
    btn->setUseDefaultSkin();
    btn->setClickCallback(fastdelegate::MakeDelegate(this, &OsuChatChannel::onChannelButtonClick));
    m_chat->m_button_container->addBaseUIElement(btn);
}

OsuChatChannel::~OsuChatChannel() {
    delete ui;
    m_chat->m_button_container->deleteBaseUIElement(btn);
}

void OsuChatChannel::onChannelButtonClick(CBaseUIButton *btn) { m_chat->switchToChannel(this); }

void OsuChatChannel::add_message(ChatMessage msg) {
    const float line_height = 20;
    const Color system_color = 0xffffff00;

    UString text_str = "";
    float x = 10;

    struct tm *tm = localtime(&msg.tms);
    auto timestamp_str = UString::format("%02d:%02d ", tm->tm_hour, tm->tm_min);
    float time_width = m_chat->font->getStringWidth(timestamp_str);
    CBaseUILabel *timestamp = new CBaseUILabel(x, y_total, time_width, line_height, "", timestamp_str);
    timestamp->setDrawFrame(false);
    timestamp->setDrawBackground(false);
    ui->getContainer()->addBaseUIElement(timestamp);
    x += time_width;

    bool is_system_message = msg.author_name.length() == 0;
    if(!is_system_message) {
        float name_width = m_chat->font->getStringWidth(msg.author_name);

        auto user_box = new OsuUIUserLabel(m_chat->m_osu, msg.author_id, msg.author_name);
        user_box->setTextColor(0xff2596be);
        user_box->setPos(x, y_total);
        user_box->setSize(name_width, line_height);
        ui->getContainer()->addBaseUIElement(user_box);
        x += name_width;

        text_str.append(": ");
    }

    // XXX: Handle links, open them in the browser on click
    // XXX: Handle map links, on click auto-download, add to song browser and select
    // XXX: Handle lobby invite links, on click join the lobby (even if passworded)
    float line_width = x;
    for(int i = 0; i < msg.text.length(); i++) {
        float char_width = m_chat->font->getGlyphMetrics(msg.text[i]).advance_x;
        if(line_width + char_width + 20 >= m_chat->getSize().x) {
            CBaseUILabel *text = new CBaseUILabel(x, y_total, line_width, line_height, "", text_str);
            text->setDrawFrame(false);
            text->setDrawBackground(false);
            if(is_system_message) {
                text->setTextColor(system_color);
            }
            ui->getContainer()->addBaseUIElement(text);
            x = 10;
            y_total += line_height;
            line_width = x;
            text_str = "";
        } else {
            text_str.append(msg.text[i]);
            line_width += char_width;
        }
    }

    CBaseUILabel *text = new CBaseUILabel(x, y_total, line_width, line_height, "", text_str);
    text->setDrawFrame(false);
    text->setDrawBackground(false);
    if(is_system_message) {
        text->setTextColor(system_color);
    }
    ui->getContainer()->addBaseUIElement(text);
    x += line_width;

    y_total += line_height;
    ui->setScrollSizeToContent();
}

void OsuChatChannel::updateLayout(Vector2 pos, Vector2 size) {
    ui->clear();
    ui->setPos(pos);
    ui->setSize(size);
    y_total = 7;

    for(auto msg : messages) {
        add_message(msg);
    }

    ui->setScrollSizeToContent();
}

OsuChat::OsuChat(Osu *osu) : OsuScreen(osu) {
    font = engine->getResourceManager()->getFont("FONT_DEFAULT");

    m_button_container = new CBaseUIContainer(0, 0, 0, 0, "");

    join_channel_btn = new OsuUIButton(osu, 0, 0, 0, 0, "button", "+");
    join_channel_btn->setUseDefaultSkin();
    join_channel_btn->setColor(0xffffff55);
    join_channel_btn->setSize(button_height + 2, button_height + 2);
    join_channel_btn->setClickCallback(fastdelegate::MakeDelegate(this, &OsuChat::askWhatChannelToJoin));
    m_button_container->addBaseUIElement(join_channel_btn);

    m_input_box = new CBaseUITextbox(0, 0, 0, 0, "");
    m_input_box->setDrawFrame(false);
    m_input_box->setDrawBackground(true);
    m_input_box->setBackgroundColor(0xdd000000);
    addBaseUIElement(m_input_box);

    updateLayout(m_osu->getScreenSize());
}

void OsuChat::draw(Graphics *g) {
    const bool isAnimating = anim->isAnimating(&m_fAnimation);
    if(!m_bVisible && !isAnimating) return;

    if(isAnimating) {
        // XXX: Setting BLEND_MODE_PREMUL_ALPHA is not enough, transparency is still incorrect
        m_osu->getSliderFrameBuffer()->enable();
        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_PREMUL_ALPHA);
    }

    if(m_selected_channel == nullptr) {
        const float chat_h = round(getSize().y * 0.3f);
        const float chat_y = getSize().y - chat_h;
        float chat_w = getSize().x;
        if(isVisibilityForced()) {
            // In the lobby and in multi rooms (e.g. when visibility is forced), don't
            // take the full horizontal width to allow for cleaner UI designs.
            chat_w = round(chat_w * 0.6);
        }

        g->setColor(COLOR(100, 0, 10, 50));
        g->fillRect(0, chat_y - (button_height + 2.f), chat_w, (button_height + 2.f));
        g->setColor(COLOR(150, 0, 0, 0));
        g->fillRect(0, chat_y, chat_w, chat_h);
    } else {
        g->setColor(COLOR(100, 0, 10, 50));
        g->fillRect(m_button_container->getPos().x, m_button_container->getPos().y, m_button_container->getSize().x,
                    m_button_container->getSize().y);
        m_button_container->draw(g);

        OsuScreen::draw(g);
        if(m_selected_channel != nullptr) {
            m_selected_channel->ui->draw(g);
        }
    }

    if(isAnimating) {
        m_osu->getSliderFrameBuffer()->disable();

        g->setBlendMode(Graphics::BLEND_MODE::BLEND_MODE_ALPHA);
        g->push3DScene(McRect(0, 0, getSize().x, getSize().y));
        {
            g->rotate3DScene(-(1.0f - m_fAnimation) * 90, 0, 0);
            g->translate3DScene(0, -(1.0f - m_fAnimation) * getSize().y * 1.25f, -(1.0f - m_fAnimation) * 700);

            m_osu->getSliderFrameBuffer()->setColor(COLORf(m_fAnimation, 1.0f, 1.0f, 1.0f));
            m_osu->getSliderFrameBuffer()->draw(g, 0, 0);
        }
        g->pop3DScene();
    }
}

void OsuChat::mouse_update(bool *propagate_clicks) {
    if(!m_bVisible) return;
    OsuScreen::mouse_update(propagate_clicks);
    m_button_container->mouse_update(propagate_clicks);
    if(m_selected_channel) {
        m_selected_channel->ui->mouse_update(propagate_clicks);
    }

    // Focus without placing the cursor at the end of the field
    m_input_box->focus(false);
}

void OsuChat::onKeyDown(KeyboardEvent &key) {
    if(!m_bVisible) return;

    // Escape: close chat
    if(key.getKeyCode() == KEY_ESCAPE) {
        if(isVisibilityForced()) return;

        key.consume();
        user_wants_chat = false;
        updateVisibility();
        return;
    }

    // Return: send message
    if(key.getKeyCode() == KEY_ENTER) {
        key.consume();
        if(m_selected_channel != nullptr && m_input_box->getText().length() > 0) {
            if(m_input_box->getText() == UString("/close")) {
                leave(m_selected_channel->name);
                return;
            }

            Packet packet = {0};
            packet.id = m_selected_channel->name[0] == '#' ? SEND_PUBLIC_MESSAGE : SEND_PRIVATE_MESSAGE;
            write_string(&packet, (char *)bancho.username.toUtf8());
            write_string(&packet, (char *)m_input_box->getText().toUtf8());
            write_string(&packet, (char *)m_selected_channel->name.toUtf8());
            write_int32(&packet, bancho.user_id);
            send_packet(packet);

            // Server doesn't echo the message back
            addMessage(m_selected_channel->name, ChatMessage{
                                                     .tms = time(NULL),
                                                     .author_id = bancho.user_id,
                                                     .author_name = bancho.username,
                                                     .text = m_input_box->getText(),
                                                 });

            m_input_box->clear();
        }
        return;
    }

    // Ctrl+W: Close current channel
    if(engine->getKeyboard()->isControlDown() && key.getKeyCode() == KEY_W) {
        key.consume();
        if(m_selected_channel != nullptr) {
            leave(m_selected_channel->name);
        }
        return;
    }

    // Ctrl+Tab: Switch channels
    // KEY_TAB doesn't work on Linux
    if(engine->getKeyboard()->isControlDown() && (key.getKeyCode() == 65056 || key.getKeyCode() == KEY_TAB)) {
        key.consume();
        if(m_selected_channel == nullptr) return;
        int chan_index = m_channels.size();
        for(auto chan : m_channels) {
            if(chan == m_selected_channel) {
                break;
            }
            chan_index++;
        }

        if(engine->getKeyboard()->isShiftDown()) {
            // Ctrl+Shift+Tab: Go to previous channel
            auto new_chan = m_channels[(chan_index - 1) % m_channels.size()];
            switchToChannel(new_chan);
        } else {
            // Ctrl+Tab: Go to next channel
            auto new_chan = m_channels[(chan_index + 1) % m_channels.size()];
            switchToChannel(new_chan);
        }
        return;
    }

    // Typing in chat: capture keypresses
    if(!engine->getKeyboard()->isAltDown()) {
        m_input_box->onKeyDown(key);
        key.consume();
        return;
    }
}

void OsuChat::onKeyUp(KeyboardEvent &key) {
    if(!m_bVisible || key.isConsumed()) return;

    m_input_box->onKeyUp(key);
    key.consume();
}

void OsuChat::onChar(KeyboardEvent &key) {
    if(!m_bVisible || key.isConsumed()) return;

    m_input_box->onChar(key);
    key.consume();
}

void OsuChat::mark_as_read(OsuChatChannel *chan) {
    if(!m_bVisible) return;

    // XXX: Only mark as read after 500ms
    chan->read = true;

    CURL *curl = curl_easy_init();
    if(!curl) {
        debugLog("Failed to initialize cURL in OsuChat::mark_as_read()!\n");
        return;
    }
    char *channel_urlencoded = curl_easy_escape(curl, chan->name.toUtf8(), 0);
    if(!channel_urlencoded) {
        debugLog("Failed to encode channel name!\n");
        curl_easy_cleanup(curl);
        return;
    }

    APIRequest request = {
        .type = MARK_AS_READ,
        .path = UString::format("/web/osu-markasread.php?u=%s&h=%s&channel=%s", bancho.username.toUtf8(),
                                bancho.pw_md5.toUtf8(), channel_urlencoded),
        .mime = NULL,
    };
    send_api_request(request);

    curl_free(channel_urlencoded);
    curl_easy_cleanup(curl);
}

void OsuChat::switchToChannel(OsuChatChannel *chan) {
    m_selected_channel = chan;
    if(!chan->read) {
        mark_as_read(m_selected_channel);
    }

    // Update button colors
    updateButtonLayout(getSize());
}

void OsuChat::addChannel(UString channel_name, bool switch_to) {
    for(auto chan : m_channels) {
        if(chan->name == channel_name) {
            if(switch_to) {
                switchToChannel(chan);
            }
            return;
        }
    }

    OsuChatChannel *chan = new OsuChatChannel(this, channel_name);
    m_channels.push_back(chan);

    if(m_selected_channel == nullptr && m_channels.size() == 1) {
        switchToChannel(chan);
    } else if(channel_name == UString("#multiplayer") || channel_name == UString("#lobby")) {
        switchToChannel(chan);
    } else if(switch_to) {
        switchToChannel(chan);
    }

    updateLayout(m_osu->getScreenSize());
}

void OsuChat::addMessage(UString channel_name, ChatMessage msg) {
    if(msg.author_id > 0 && channel_name[0] != '#' && msg.author_name != bancho.username) {
        // If it's a PM, the channel title should be the one who sent the message
        channel_name = msg.author_name;
    }

    addChannel(channel_name);
    for(auto chan : m_channels) {
        if(chan->name != channel_name) continue;
        chan->messages.push_back(msg);
        chan->add_message(msg);
        chan->read = false;
        if(chan == m_selected_channel) {
            mark_as_read(chan);
        } else {
            updateButtonLayout(getSize());
        }
        if(chan->messages.size() > 100) {
            chan->messages.erase(chan->messages.begin());
        }
        return;
    }
}

void OsuChat::removeChannel(UString channel_name) {
    OsuChatChannel *chan = nullptr;
    for(auto c : m_channels) {
        if(c->name == channel_name) {
            chan = c;
            break;
        }
    }

    if(chan == nullptr) return;

    auto it = std::find(m_channels.begin(), m_channels.end(), chan);
    m_channels.erase(it);
    if(m_selected_channel == chan) {
        m_selected_channel = nullptr;
        if(!m_channels.empty()) {
            switchToChannel(m_channels[0]);
        }
    }

    delete chan;
    updateButtonLayout(getSize());
}

void OsuChat::updateLayout(Vector2 newResolution) {
    // In the lobby and in multi rooms (e.g. when visibility is forced), don't
    // take the full horizontal width to allow for cleaner UI designs.
    if(isVisibilityForced()) {
        newResolution.x = round(newResolution.x * 0.6);
    }

    setSize(newResolution);

    const float chat_w = newResolution.x;
    const float chat_h = round(newResolution.y * 0.3f) - input_box_height;
    const float chat_y = newResolution.y - (chat_h + input_box_height);
    for(auto chan : m_channels) {
        chan->updateLayout(Vector2{0.f, chat_y}, Vector2{chat_w, chat_h});
    }

    m_input_box->setPos(Vector2{0.f, chat_y + chat_h});
    m_input_box->setSize(Vector2{chat_w, input_box_height});

    if(m_selected_channel == nullptr && !m_channels.empty()) {
        m_selected_channel = m_channels[0];
        m_selected_channel->read = true;
    }

    updateButtonLayout(newResolution);
    updateButtonLayout(newResolution);  // idk
}

void OsuChat::updateButtonLayout(Vector2 screen) {
    const float initial_x = 2;
    float total_x = initial_x;

    std::sort(m_channels.begin(), m_channels.end(),
              [](OsuChatChannel *a, OsuChatChannel *b) { return a->name < b->name; });

    // Look, I really tried. But for some reason setRelPos() doesn't work until we change
    // the screen resolution once. So I'll just compute absolute position manually.
    float button_container_height = button_height + 2;
    for(auto chan : m_channels) {
        OsuUIButton *btn = chan->btn;
        float button_width = font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            button_container_height += button_height;
        }

        total_x += button_width;
    }

    const float chat_y = round(screen.y * 0.7f);
    float total_y = 0.f;
    total_x = initial_x;
    for(auto chan : m_channels) {
        OsuUIButton *btn = chan->btn;
        float button_width = font->getStringWidth(btn->getText()) + 20;

        // Wrap channel buttons
        if(total_x + button_width > screen.x - 20) {
            total_x = initial_x;
            total_y += button_height;
        }

        btn->setPos(total_x, chat_y - button_container_height + total_y);
        // Buttons are drawn a bit smaller than they should, so we up the size here
        btn->setSize(button_width + 2, button_height + 2);

        if(m_selected_channel->name == btn->getText()) {
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

    join_channel_btn->setPos(total_x, chat_y - button_container_height + total_y);
    m_button_container->setPos(0, chat_y - button_container_height);
    m_button_container->setSize(screen.x, button_container_height);
}

void OsuChat::join(UString channel_name) {
    // XXX: Open the channel immediately, without letting the user send messages in it.
    //      Would require a way to signal if a channel is joined or not.
    //      Would allow to keep open the tabs of the channels we got kicked out of.
    Packet packet = {0};
    packet.id = CHANNEL_JOIN;
    write_string(&packet, channel_name.toUtf8());
    send_packet(packet);
}

void OsuChat::leave(UString channel_name) {
    bool send_leave_packet = channel_name[0] == '#';
    if(channel_name == UString("#lobby")) send_leave_packet = false;
    if(channel_name == UString("#multiplayer")) send_leave_packet = false;

    if(send_leave_packet) {
        Packet packet = {0};
        packet.id = CHANNEL_PART;
        write_string(&packet, channel_name.toUtf8());
        send_packet(packet);
    }

    removeChannel(channel_name);
}

void OsuChat::onDisconnect() {
    for(auto chan : m_channels) {
        delete chan;
    }
    m_channels.clear();

    for(auto chan : chat_channels) {
        delete chan.second;
    }
    chat_channels.clear();

    m_selected_channel = nullptr;
    updateLayout(m_osu->getScreenSize());

    updateVisibility();
}

void OsuChat::onResolutionChange(Vector2 newResolution) { updateLayout(newResolution); }

bool OsuChat::isVisibilityForced() {
    if(m_osu->m_room == nullptr || m_osu->m_lobby == nullptr || m_osu->m_songBrowser2 == nullptr) return false;
    bool sitting_in_room =
        m_osu->m_room->isVisible() && !m_osu->m_songBrowser2->isVisible() && !bancho.is_playing_a_multi_map();
    bool sitting_in_lobby = m_osu->m_lobby->isVisible();
    bool is_forced = (sitting_in_room || sitting_in_lobby);

    if(is_forced != visibility_was_forced) {
        // Chat width changed: update the layout now
        visibility_was_forced = is_forced;
        updateLayout(m_osu->getScreenSize());
    }
    return is_forced;
}

void OsuChat::updateVisibility() {
    auto selected_beatmap = m_osu->getSelectedBeatmap();
    bool can_skip = (selected_beatmap != nullptr) && (selected_beatmap->isInSkippableSection());
    bool is_clicking_circles =
        m_osu->isInPlayMode() && !can_skip && !m_osu->m_bModAuto && !m_osu->m_pauseMenu->isVisible();
    if(bancho.is_playing_a_multi_map() && !bancho.room.all_players_loaded) {
        is_clicking_circles = false;
    }
    bool force_hide = m_osu->m_optionsMenu->isVisible() || m_osu->m_modSelector->isVisible() || is_clicking_circles;
    if(!bancho.is_online()) force_hide = true;

    if(force_hide) {
        setVisible(false);
    } else if(isVisibilityForced()) {
        setVisible(true);
    } else {
        setVisible(user_wants_chat);
    }
}

CBaseUIContainer *OsuChat::setVisible(bool visible) {
    if(visible == m_bVisible) return this;

    if(visible && bancho.user_id <= 0) {
        m_osu->m_optionsMenu->askForLoginDetails();
        return this;
    }

    m_bVisible = visible;
    if(visible) {
        m_osu->m_optionsMenu->setVisible(false);
        anim->moveQuartOut(&m_fAnimation, 1.0f, 0.25f * (1.0f - m_fAnimation), true);
    } else {
        anim->moveQuadOut(&m_fAnimation, 0.0f, 0.25f * m_fAnimation, true);
    }

    return this;
}

bool OsuChat::isMouseInChat() {
    if(!isVisible()) return false;
    if(m_selected_channel == nullptr) return false;
    return m_input_box->isMouseInside() || m_selected_channel->ui->isMouseInside();
}

void OsuChat::askWhatChannelToJoin(CBaseUIButton *btn) {
    // XXX: Could display nicer UI with full channel list (chat_channels in Bancho.cpp)
    m_osu->m_prompt->prompt("Type in the channel you want to join (e.g. '#osu'):",
                            fastdelegate::MakeDelegate(this, &OsuChat::join));
}
