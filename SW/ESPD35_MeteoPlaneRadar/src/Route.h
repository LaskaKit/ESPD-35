// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Where the selected aircraft is flying from and to (adsbdb.com).
//
//  adsb.fi carries positions but no route, so this has to come from elsewhere.
//  adsbdb.com is a free lookup database, no key, no registration: callsign ->
//  departure and arrival airport, ICAO hex -> registration, type and operator.
//
//  It is asked ONLY when the user opens an aircraft's detail - one request for
//  one aircraft, never for the whole list - and the answer is cached, so
//  flicking between two aircraft does not hit the API again. Plenty of flights
//  have no route on file (general aviation, military, helicopters); that is a
//  normal outcome, not an error, and simply shows nothing.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 (autor Petr / chiptron.cz)
//  a upraveno pro ESPD-3.5 (ILI9488 480x320, SPI).
// =============================================================================
#pragma once
#include <Arduino.h>

enum RouteState : uint8_t {
  ROUTE_IDLE = 0,   // nothing asked for
  ROUTE_WAIT,       // queued / being fetched
  ROUTE_OK,         // something was found
  ROUTE_NONE        // asked, but this flight has no route on file
};

struct RouteInfo {
  char from[20] = "";   // "London" or "LHR"
  char to[20]   = "";
  char reg[12]  = "";   // registration, e.g. EI-EJG
  char type[20] = "";   // e.g. "A330 202"
};

// Ask about this aircraft. Cheap and idempotent: repeated calls with the same
// callsign do nothing once the answer is in the cache. An empty callsign (many
// aircraft do not broadcast one) only looks up the airframe by hex.
void       Route_Select(const char* callsign, const char* hex);

// Nothing is selected any more - stop any pending lookup.
void       Route_Clear();

// Runs the pending lookup. Call from loop(); does nothing when there is none.
void       Route_Tick();

RouteState Route_GetState();
const RouteInfo* Route_Get();   // valid while the state is ROUTE_OK
