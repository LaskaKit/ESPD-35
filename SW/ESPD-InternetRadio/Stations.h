/*
 * Stations.h – seznam stanic
 * Přidej/uber řádek podle libosti. Podporované formáty: MP3, AAC, OPUS, VORBIS, FLAC.
 * Názvy můžou obsahovat diakritiku – na displeji se převede na ASCII.
 * (URL ověřené 10/2026)
 */
#pragma once

struct Station {
  const char* name;
  const char* url;
};

const Station STATIONS[] = {
  {"Radiožurnál",  "https://rozhlas.stream/radiozurnal_mp3_128.mp3"},
  {"ČRo Dvojka",   "https://rozhlas.stream/dvojka_mp3_128.mp3"},
  {"ČRo Vltava",   "https://rozhlas.stream/vltava_mp3_128.mp3"},
  {"ČRo Jazz",     "https://rozhlas.stream/jazz_aac_128.aac"},
  {"Rock Radio",   "https://playerservices.streamtheworld.com/api/livestream-redirect/ROCK_RADIO_128.mp3"},
  {"Radio Beat",   "https://icecast2.play.cz/radiobeat128.mp3"},
  {"Frekvence 1",  "https://ice.actve.net/fm-frekvence1-aac"},
  {"Evropa 2",     "https://ice.actve.net/fm-evropa2-128"},
  {"Kiss",         "https://icecast7.play.cz/kiss128.mp3"},
};

constexpr int NUM_STATIONS = sizeof(STATIONS) / sizeof(STATIONS[0]);
