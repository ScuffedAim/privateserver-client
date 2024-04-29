#include "BanchoProtocol.h"

#include <stdlib.h>
#include <string.h>

#include "Bancho.h"

Room::Room() {
    // 0-initialized room means we're not in multiplayer at the moment
}

Room::Room(Packet *packet) {
    id = read_short(packet);
    in_progress = read_byte(packet);
    match_type = read_byte(packet);
    mods = read_int32(packet);
    name = read_string(packet);

    has_password = read_byte(packet) > 0;
    if(has_password) {
        // Discard password. It should be an empty string, but just in case, read it properly.
        packet->pos--;
        read_string(packet);
    }

    map_name = read_string(packet);
    map_id = read_int32(packet);

    auto hash_str = read_string(packet);
    map_md5 = hash_str.toUtf8();

    nb_players = 0;
    for(int i = 0; i < 16; i++) {
        slots[i].status = read_byte(packet);
    }
    for(int i = 0; i < 16; i++) {
        slots[i].team = read_byte(packet);
    }
    for(int s = 0; s < 16; s++) {
        if(!slots[s].is_locked()) {
            nb_open_slots++;
        }

        if(slots[s].has_player()) {
            slots[s].player_id = read_int32(packet);
            nb_players++;
        }
    }

    host_id = read_int32(packet);
    mode = read_byte(packet);
    win_condition = read_byte(packet);
    team_type = read_byte(packet);
    freemods = read_byte(packet);
    if(freemods) {
        for(int i = 0; i < 16; i++) {
            slots[i].mods = read_int32(packet);
        }
    }

    seed = read_int32(packet);
}

void Room::pack(Packet *packet) {
    write_short(packet, id);
    write_byte(packet, in_progress);
    write_byte(packet, match_type);
    write_int32(packet, mods);
    write_string(packet, name.toUtf8());
    write_string(packet, password.toUtf8());
    write_string(packet, map_name.toUtf8());
    write_int32(packet, map_id);
    write_string(packet, map_md5.toUtf8());
    for(int i = 0; i < 16; i++) {
        write_byte(packet, slots[i].status);
    }
    for(int i = 0; i < 16; i++) {
        write_byte(packet, slots[i].team);
    }
    for(int s = 0; s < 16; s++) {
        if(slots[s].has_player()) {
            write_int32(packet, slots[s].player_id);
        }
    }

    write_int32(packet, host_id);
    write_byte(packet, mode);
    write_byte(packet, win_condition);
    write_byte(packet, team_type);
    write_byte(packet, freemods);
    if(freemods) {
        for(int i = 0; i < 16; i++) {
            write_int32(packet, slots[i].mods);
        }
    }

    write_int32(packet, seed);
}

bool Room::is_host() { return host_id == bancho.user_id; }

void read_bytes(Packet *packet, uint8_t *bytes, size_t n) {
    if(packet->pos + n > packet->size) {
        packet->pos = packet->size + 1;
        return;
    }
    memcpy(bytes, packet->memory + packet->pos, n);
    packet->pos += n;
}

uint8_t read_byte(Packet *packet) {
    uint8_t byte = 0;
    read_bytes(packet, &byte, 1);
    return byte;
}

uint16_t read_short(Packet *packet) {
    uint16_t s = 0;
    read_bytes(packet, (uint8_t *)&s, 2);
    return s;
}

uint32_t read_int32(Packet *packet) {
    uint32_t i = 0;
    read_bytes(packet, (uint8_t *)&i, 4);
    return i;
}

uint64_t read_int64(Packet *packet) {
    uint64_t i = 0;
    read_bytes(packet, (uint8_t *)&i, 8);
    return i;
}

uint32_t read_uleb128(Packet *packet) {
    uint32_t result = 0;
    uint32_t shift = 0;
    uint8_t byte = 0;

    do {
        byte = read_byte(packet);
        result |= (byte & 0x7f) << shift;
        shift += 7;
    } while(byte & 0x80);

    return result;
}

float read_float32(Packet *packet) {
    float f = 0;
    read_bytes(packet, (uint8_t *)&f, 4);
    return f;
}

double read_float64(Packet *packet) {
    double f = 0;
    read_bytes(packet, (uint8_t *)&f, 8);
    return f;
}

UString read_string(Packet *packet) {
    uint8_t empty_check = read_byte(packet);
    if(empty_check == 0) return UString("");

    uint32_t len = read_uleb128(packet);
    uint8_t *str = new uint8_t[len + 1];
    read_bytes(packet, str, len);
    str[len] = '\0';

    auto ustr = UString((const char *)str);
    delete[] str;

    return ustr;
}

std::string read_stdstring(Packet *packet) {
    uint8_t empty_check = read_byte(packet);
    if(empty_check == 0) return std::string();

    uint32_t len = read_uleb128(packet);
    uint8_t *str = new uint8_t[len + 1];
    read_bytes(packet, str, len);

    std::string str_out((const char *)str, len);
    delete[] str;

    return str_out;
}

MD5Hash read_hash(Packet *packet) {
    MD5Hash hash;

    uint8_t empty_check = read_byte(packet);
    if(empty_check == 0) return hash;

    uint32_t len = read_uleb128(packet);
    if(len > 32) {
        len = 32;
    }

    read_bytes(packet, (uint8_t *)hash.hash, len);
    hash.hash[len] = '\0';
    return hash;
}

void skip_string(Packet *packet) {
    uint8_t empty_check = read_byte(packet);
    if(empty_check == 0) {
        return;
    }

    uint32_t len = read_uleb128(packet);
    packet->pos += len;
}

void write_bytes(Packet *packet, uint8_t *bytes, size_t n) {
    if(packet->pos + n > packet->size) {
        packet->memory = (unsigned char *)realloc(packet->memory, packet->size + n + 128);
        packet->size += n + 128;
        if(!packet->memory) return;
    }

    memcpy(packet->memory + packet->pos, bytes, n);
    packet->pos += n;
}

void write_byte(Packet *packet, uint8_t b) { write_bytes(packet, &b, 1); }

void write_short(Packet *packet, uint16_t s) { write_bytes(packet, (uint8_t *)&s, 2); }

void write_int32(Packet *packet, uint32_t i) { write_bytes(packet, (uint8_t *)&i, 4); }

void write_int64(Packet *packet, uint64_t i) { write_bytes(packet, (uint8_t *)&i, 8); }

void write_uleb128(Packet *packet, uint32_t num) {
    if(num == 0) {
        uint8_t zero = 0;
        write_byte(packet, zero);
        return;
    }

    while(num != 0) {
        uint8_t next = num & 0x7F;
        num >>= 7;
        if(num != 0) {
            next |= 0x80;
        }
        write_byte(packet, next);
    }
}

void write_float32(Packet *packet, float f) { write_bytes(packet, (uint8_t *)&f, 4); }

void write_float64(Packet *packet, double f) { write_bytes(packet, (uint8_t *)&f, 8); }

void write_string(Packet *packet, const char *str) {
    if(!str || str[0] == '\0') {
        uint8_t zero = 0;
        write_byte(packet, zero);
        return;
    }

    uint8_t empty_check = 11;
    write_byte(packet, empty_check);

    uint32_t len = strlen(str);
    write_uleb128(packet, len);
    write_bytes(packet, (uint8_t *)str, len);
}
