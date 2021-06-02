/*
    Part of Grbl_ESP32
    2021 -  Stefan de Bruijn

    Grbl_ESP32 is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Grbl_ESP32 is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Grbl_ESP32.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "Pins/PinDetail.h"
#include "Pins/PinCapabilities.h"
#include "Pins/PinAttributes.h"
#include "StringRange.h"

#include <Arduino.h>  // for IRAM_ATTR
#include <cstdint>
#include <cstring>
#include <utility>
#include "Assert.h"

// TODO: ENABLE_CONTROL_SW_DEBOUNCE should end up here with a shared task.

// #define PIN_DEBUG  // Pin debugging. WILL spam you with a lot of data!

// Forward declarations:
class String;

// Pin class. A pin is basically a thing that can 'output', 'input' or do both. GPIO on an ESP32 comes to mind,
// but there are way more possible pins. Think about I2S/I2C/SPI extenders, RS485 driven pin devices and even
// WiFi wall sockets.
//
// Normally people define a pin using the yaml configuration, and specify there how a pin should behave. For
// example, ':low' is normally supported, which means 'default low'. For output pins this is relevant, because
// it basically means 'off is a high (3.3V) signal, on is a low (GND) signal'. By doing it like this, there is
// no longer a need for invert flags, and things like that.
//
// Pins are supposed to be fields during the lifetime of the MachineConfig. In normal operations, the pin is
// created by the parser (using the 'create' methods), then kept in fields from the configurable, and eventually
// cleaned up by the destructor. Pins cannot be copied, there always has to be 1 owner of a pin, and they shouldn't
// be thrown around in the application.
//
// Pins internally use PinDetail classes. PinDetail's are just implementation details for a certain type of pin.
// PinDetail is not exposed to developers, because they should never be used directly. Pin class is your
// one-stop-go-to-shop for an pin.
class Pin {
    // Helper for handling callbacks and mapping them to the proper class:
    template <typename ThisType, void (ThisType::*Callback)()>
    struct InterruptCallbackHelper {
        static void IRAM_ATTR callback(void* ptr) { (static_cast<ThisType*>(ptr)->*Callback)(); }
    };

    // Helper for handling callbacks and mapping them to the proper class. This one is just meant
    // for backward compatibility:
    template <void (*Callback)()>
    struct InterruptCallbackHelper2 {
        static void IRAM_ATTR callback(void* /*ptr*/) { Callback(); }
    };

    // The undefined pin and error pin are two special pins. Error pins always throw an error when they are used.
    // These are useful for unit testing, and for initializing pins that _always_ have to be defined by a user
    // (or else). Undefined pins are basically pins with no functionality. They don't have to be defined, but also
    // have no functionality when they are used.
    static Pins::PinDetail* undefinedPin;
    static Pins::PinDetail* errorPin;

    // Implementation details of this pin.
    Pins::PinDetail* _detail;

    static bool parse(StringRange str, Pins::PinDetail*& detail);

    inline Pin(Pins::PinDetail* detail) : _detail(detail) {}

public:
    using Capabilities = Pins::PinCapabilities;
    using Attr         = Pins::PinAttributes;

    // A default pin is an undefined pin.
    inline Pin() : _detail(undefinedPin) {}

    static const bool On  = true;
    static const bool Off = false;

    // inline static Pins::PinDetail* create(const char* str) { return create(StringRange(str)); };

    static Pin  create(const char* str) { return create(StringRange(str)); }  // ensure it's not ambiguous
    static Pin  create(const StringRange& str);
    static Pin  create(const String& str);
    static bool validate(const String& str);

    // We delete the copy constructor, and implement the move constructor. The move constructor is required to support
    // the correct execution of 'return' in f.ex. `create` calls. It basically transfers ownership from the callee to the
    // caller of the Pin.
    inline Pin(const Pin& o) = delete;
    inline Pin(Pin&& o) : _detail(nullptr) { std::swap(_detail, o._detail); }

    inline Pin& operator=(const Pin& o) = delete;
    inline Pin& operator                =(Pin&& o) {
        std::swap(_detail, o._detail);
        return *this;
    }

    // Some convenience operators and functions:
    inline bool operator==(const Pin& o) const { return _detail == o._detail; }
    inline bool operator!=(const Pin& o) const { return _detail != o._detail; }

    inline bool undefined() const { return _detail == undefinedPin; }
    inline bool defined() const { return !undefined(); }

    // External libraries normally use digitalWrite, digitalRead and setMode. Since we cannot handle that behavior, we
    // just give back the uint8_t for getNative.
    inline uint8_t getNative(Capabilities expectedBehavior) const {
        Assert(_detail->capabilities().has(expectedBehavior), "Requested pin does not have the expected behavior.");
        return _detail->_index;
    }

    inline void write(bool value) const { _detail->write(value); }

    inline bool read() const { return _detail->read() != 0; }

    inline void setAttr(Attr attributes) const { _detail->setAttr(attributes); }

    inline Attr getAttr() const { return _detail->getAttr(); }

    inline void on() const { write(1); }
    inline void off() const { write(0); }

    static Pin Error() { return Pin(errorPin); }

    // ISR handlers. Map methods on 'this' types.

    template <typename ThisType, void (ThisType::*Callback)()>
    void attachInterrupt(ThisType* arg, int mode) {
        _detail->attachInterrupt(InterruptCallbackHelper<ThisType, Callback>::callback, arg, mode);
    }

    // Backward compatibility ISR handler:
    void attachInterrupt(void (*callback)(void*), int mode, void* arg = nullptr) const { _detail->attachInterrupt(callback, arg, mode); }

    void detachInterrupt() const { _detail->detachInterrupt(); }

    // Other functions:
    Capabilities capabilities() const { return _detail->capabilities(); }

    inline String name() const { return _detail->toString(); }

    void report(const char* legend);

    inline void swap(Pin& o) { std::swap(o._detail, _detail); }

    ~Pin();
};
