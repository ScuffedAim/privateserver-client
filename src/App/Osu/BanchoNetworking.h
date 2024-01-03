#pragma once
#include "BanchoProtocol.h"
#include <curl/curl.h>
#include <string>

// TODO @kiwec: Don't hardcode this
// User agent sent when downloading beatmaps
#define MCOSU_VERSION "34.00-dev"
#define MCOSU_USER_AGENT "Mozilla/5.0 (compatible; McOsu/" MCOSU_VERSION "; +https://mcosu.kiwec.net/)"

enum APIRequestType {
  GET_MAP_LEADERBOARD,
  GET_BEATMAPSET_INFO,
};

struct APIRequest {
  APIRequestType type;
  std::string path;
  uint8_t *extra;
};

void disconnect();
void reconnect();

// Send an API request.
void send_api_request(APIRequest request);

// Send a packet to Bancho. Do not free it after calling this.
void send_packet(Packet& packet);

// Poll for new packets. Should be called regularly from main thread.
void receive_api_responses();
void receive_bancho_packets();

// Initialize networking thread. Should be called once when starting McOsu.
void init_networking_thread();
