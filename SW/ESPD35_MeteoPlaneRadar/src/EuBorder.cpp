// =============================================================================
//  ESPD35_MeteoPlaneRadar
//  Map underlay: European country outlines and cities.
//
//  Prevzato z petus/MeteoPlaneRadar v0.6.1 (autor Petr / chiptron.cz).
// =============================================================================
#include "EuBorder.h"
#include "EuMapData.h"
#include "CzCitiesData.h"   // Czech cities - they replace the European ones
#include "UI.h"
#include "Layout.h"
#include <string.h>
#include <math.h>

// Decode a fixed-point vertex back into degrees.
static inline float ptLon(uint16_t v) { return EU_LON_ORIGIN + v * EU_COORD_SCALE; }
static inline float ptLat(uint16_t v) { return EU_LAT_ORIGIN + v * EU_COORD_SCALE; }

void EuBorder_Draw(ProjectFn project, uint16_t color,
                   float lat0, float lat1, float lon0, float lon1) {
  const int nRings = EU_RING_COUNT;

  for (int r = 0; r < nRings; r++) {
    uint16_t start = EU_RING_OFFSETS[r];
    uint16_t end   = EU_RING_OFFSETS[r + 1];   // exclusive
    if (end - start < 2) continue;

    // Ring-level rejection: if the ring's bounding box misses the window
    // entirely, skip the whole country in one go. This is what keeps the
    // draw cheap - at 25 km over Prague, nearly every ring dies right here.
    uint16_t bxmin = 0xFFFF, bxmax = 0, bymin = 0xFFFF, bymax = 0;
    for (uint16_t i = start; i < end; i++) {
      uint16_t x = EU_BORDER_PTS[i][0], y = EU_BORDER_PTS[i][1];
      if (x < bxmin) bxmin = x;
      if (x > bxmax) bxmax = x;
      if (y < bymin) bymin = y;
      if (y > bymax) bymax = y;
    }
    if (ptLon(bxmax) < lon0 || ptLon(bxmin) > lon1) continue;
    if (ptLat(bymax) < lat0 || ptLat(bymin) > lat1) continue;

    // Segment level: draw the segment i-1 -> i whenever *either* endpoint lies
    // in the window. Requiring both would chop off the border exactly where it
    // crosses the edge of the view, leaving visible gaps at the screen rim.
    //
    // Every vertex of a surviving ring is projected once and cached in prevX/
    // prevY, so a point sitting just outside the window is still available as
    // the far end of the segment that reaches into it.
    int   prevX = 0, prevY = 0;
    bool  prevIn = false;
    bool  first = true;

    for (uint16_t i = start; i < end; i++) {
      float lon = ptLon(EU_BORDER_PTS[i][0]);
      float lat = ptLat(EU_BORDER_PTS[i][1]);
      bool  in  = (lat >= lat0 && lat <= lat1 && lon >= lon0 && lon <= lon1);

      int sx, sy;
      project(lat, lon, &sx, &sy);

      if (!first && (in || prevIn)) {
        gfx->drawLine(prevX, prevY, sx, sy, color);
      }

      prevX = sx; prevY = sy;
      prevIn = in;
      first = false;
    }
  }
}

// Do we have this place in our own Czech list? Matched on POSITION, not on the
// name: our list writes some of them differently ("Usti n. L." against the
// European "Usti nad Labem") and a name comparison would let those through
// twice. Verified against the real data - all 21 overlaps pair up one to one,
// and no two distinct towns fall within the tolerance of each other.
static bool haveCzechCity(float lat, float lon) {
  const float TOL = 0.05f;                 // about 5 km
  for (int i = 0; i < CZ_CITY_COUNT; i++) {
    if (fabsf(CZ_CITIES[i].lat - lat) <= TOL &&
        fabsf(CZ_CITIES[i].lon - lon) <= TOL) return true;
  }
  return false;
}

// --- Label collision ---------------------------------------------------------
// At the widest ranges the labels start sitting on top of each other and the
// map turns into mush. Every label that gets drawn claims a rectangle; a later
// one that would overlap it is dropped entirely, dot and all - half a name
// under another name is worse than no name.
//
// Since 0.6.0 the rectangles are held by the shared Layout module rather than
// by a private array here. That is what stops a city label from landing on the
// clock or the altitude legend: the screen reserves its chrome before the map
// is drawn, so those rectangles are already taken by the time the first city
// asks for space.
//
// Cities are drawn tier by tier, so the bigger place always wins the spot and
// it is the small town next to it that disappears.

// One city. Split out of the loop below so the European set and the Czech
// list can share it.
static void drawOneCity(const EuCity& c, ProjectFn project, int cx, int cy,
                        long r2, uint16_t dotColor, uint16_t textColor,
                        bool showFull, uint8_t tier,
                        float lat0, float lat1, float lon0, float lon1);

void EuBorder_DrawCities(ProjectFn project, int cx, int cy, int radius,
                         uint16_t dotColor, uint16_t textColor,
                         bool showFull, uint8_t maxTier,
                         float lat0, float lat1, float lon0, float lon1) {
  long r2 = (long)radius * radius;

  // Tier by tier, biggest first - whoever claims the space first keeps it.
  for (uint8_t tier = 1; tier <= maxTier; tier++) {
    for (int i = 0; i < EU_CITY_COUNT; i++) {
      const EuCity& c = EU_CITIES[i];
      // Czech cities come from our own list instead - same places, but with the
      // abbreviations people actually recognise (PHA, not PRAH). Only places we
      // really do have are skipped, so Dresden or Wroclaw, which fall inside the
      // same rectangle, still get drawn from the European data.
      if (c.lat >= CZ_BOX_LAT0 && c.lat <= CZ_BOX_LAT1 &&
          c.lon >= CZ_BOX_LON0 && c.lon <= CZ_BOX_LON1 &&
          haveCzechCity(c.lat, c.lon)) continue;
      drawOneCity(c, project, cx, cy, r2, dotColor, textColor,
                  showFull, tier, lat0, lat1, lon0, lon1);
    }
    for (int i = 0; i < CZ_CITY_COUNT; i++)
      drawOneCity(CZ_CITIES[i], project, cx, cy, r2, dotColor, textColor,
                  showFull, tier, lat0, lat1, lon0, lon1);
  }
}

static void drawOneCity(const EuCity& c, ProjectFn project, int cx, int cy,
                        long r2, uint16_t dotColor, uint16_t textColor,
                        bool showFull, uint8_t tier,
                        float lat0, float lat1, float lon0, float lon1) {
  {

    // This pass draws exactly one tier - see the loop in the caller.
    if (c.tier != tier) return;

    // Geographic cull before projecting.
    if (c.lat < lat0 || c.lat > lat1 || c.lon < lon0 || c.lon > lon1) return;

    int sx, sy;
    project(c.lat, c.lon, &sx, &sy);

    // Skip anything outside the display circle.
    long dx = sx - cx, dy = sy - cy;
    if (dx * dx + dy * dy > r2) return;

    // Where the label goes: to the right of the dot, or to the left when that
    // would spill out of the circle.
    const char* label = showFull ? c.name : c.abbr;
    int tw = strlen(label) * 6;
    int ty = sy - 4;
    int tx = sx + 9;
    if ((long)(tx + tw - cx) * (tx + tw - cx) + dy * dy > r2) tx = sx - 9 - tw;

    // Claim the space, dot included, with two pixels of air around it. If it is
    // taken, try the other side of the dot before giving up.
    // Claim the space, dot included, with two pixels of air around it. If it is
    // taken, try the other side of the dot before giving up.
    int bx0 = (tx < sx) ? (tx - 2) : (sx - 5);
    int bx1 = (tx < sx) ? (sx + 5) : (tx + tw + 2);
    if (!Layout_Claim(bx0, ty - 2, bx1 - bx0 + 1, 12)) {
      int alt = (tx > sx) ? (sx - 9 - tw) : (sx + 9);
      int ax0 = (alt > sx) ? (sx - 5) : (alt - 2);
      int ax1 = (alt > sx) ? (alt + tw + 2) : (sx + 5);
      if (!Layout_Claim(ax0, ty - 2, ax1 - ax0 + 1, 12)) return;   // both sides busy
      tx = alt;
    }

    gfx->fillCircle(sx, sy, 3, dotColor);
    gfx->setTextSize(1);
    gfx->setTextColor(textColor);
    gfx->setCursor(tx, ty);
    gfx->print(label);
  }
}
