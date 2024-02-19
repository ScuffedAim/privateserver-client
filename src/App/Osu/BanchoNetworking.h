#pragma once
#include "BanchoProtocol.h"
#include <string>
#include <curl/curl.h>

#define MCOSU_STREAM "dev"
#define MCOSU_UPDATE_URL "https://mcosu.kiwec.net"

// NOTE: Full version can be something like "b20200201.2cuttingedge"
#define OSU_VERSION "b20240123"
#define OSU_VERSION_DATEONLY 20240123

enum APIRequestType {
  GET_MAP_LEADERBOARD,
  GET_BEATMAPSET_INFO,
  MARK_AS_READ,
  SUBMIT_SCORE,
};

struct APIRequest {
  APIRequestType type;
  UString path;
  curl_mime *mime;
  uint8_t *extra;
  uint32_t extra_int; // lazy
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

size_t curl_write(void *contents, size_t size, size_t nmemb, void *userp);

extern UString cho_token;
