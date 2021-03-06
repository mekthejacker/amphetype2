// Copyright (C) 2016  Cory Parsons
//
// This file is part of amphetype2.
//
// amphetype2 is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// amphetype2 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with amphetype2.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef SRC_DEFS_H_
#define SRC_DEFS_H_

#include <QMetaType>
#include <QtPlugin>

class AmphetypeWindow {
public:
  virtual void onProfileChange() = 0;
  virtual void loadSettings() {}
  virtual void saveSettings() {}
};
Q_DECLARE_INTERFACE(AmphetypeWindow, "AmphetypeWindow")

namespace amphetype {

enum class SelectionMethod {
  None = -1,
  Random = 0,
  InOrder,
  Repeat,
  SlowWords,
  FastWords,
  ViscousWords,
  FluidWords,
  InaccurateWords,
  AccurateWords,
  DamagingWords
};

enum SaveFlags {
  None = 0x0,
  SaveResults = 0x1,
  SaveStatistics = 0x2,
  SaveMistakes = 0x4
};

enum class text_type { Standard = 0, Lesson, GeneratedFromStatistics };

namespace statistics {
enum class Order {
  Slow = 0,
  Fast,
  Viscous,
  Fluid,
  Inaccurate,
  Accurate,
  Mistakes,
  Total,
  Damaging
};

enum class Type { Keys = 0, Trigrams, Words };
};  // namespace statistics

enum class Layout {
  QWERTY = 0,
  QWERTZ,
  AZERTY,
  WORKMAN,
  COLEMAK,
  DVORAK,
  CUSTOM
};

enum class Standard { NONE = -1, ANSI = 0, ISO };

enum class Modifier { None, Shift, AltGr };

enum class KeyboardRow { UPPER, MIDDLE, LOWER };

enum class Finger { INDEX_INNER, INDEX, MIDDLE, RING, PINKY, PINKY_EXTRA };

// The sizes of the key caps in the standard 5x15 area on most keyboards.
namespace standards {
static constexpr const double ansi_keys[][15] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5, 0},
    {1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.25, 0, 0},
    {2.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.75, 0, 0, 0},
    {1.25, 1.25, 1.25, 6.25, 1.25, 1.25, 1.25, 1.25, 0, 0, 0, 0, 0, 0, 0}};
static constexpr const int ansi_offset[5] = {0, 1, 1, 1, 3};

static constexpr const double iso_keys[][15] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 0},
    {1.5, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.5, 0},
    {1.75, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1.25, 0},
    {1.25, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2.75, 0, 0},
    {1.25, 1.25, 1.25, 6.25, 1.25, 1.25, 1.25, 1.25, 0, 0, 0, 0, 0, 0, 0}};
static constexpr const int iso_offset[5] = {0, 1, 1, 1, 3};

}  // namespace standards
}  // namespace amphetype

Q_DECLARE_METATYPE(amphetype::SelectionMethod);
Q_DECLARE_METATYPE(amphetype::text_type);
Q_DECLARE_METATYPE(amphetype::statistics::Order);
Q_DECLARE_METATYPE(amphetype::statistics::Type);
Q_DECLARE_METATYPE(amphetype::Layout);
Q_DECLARE_METATYPE(amphetype::Standard);
Q_DECLARE_METATYPE(amphetype::Modifier);

Q_DECLARE_METATYPE(amphetype::KeyboardRow);
Q_DECLARE_METATYPE(amphetype::Finger);

#endif  // SRC_DEFS_H_
