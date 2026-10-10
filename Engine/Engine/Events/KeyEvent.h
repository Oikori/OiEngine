#pragma once

#include <sstream>

#include "Event.h"
#include "Engine/Core/KeyCodes.h"

namespace OiEngine {
	class KeyEvent : public Event {
	public:
		KeyCode GetKeyCode() const { return m_keyCode; }

		EVENT_CLASS_CATEGORY(EventCategoryKeyboard | EventCategoryInput)
	protected:
        explicit KeyEvent(const KeyCode keycode): m_keyCode(keycode) {}

		KeyCode m_keyCode;
	};

	class KeyPressedEvent : public KeyEvent {
	public:
        explicit KeyPressedEvent(const KeyCode keycode, bool repeat = false)
	        : KeyEvent(keycode), m_repeat(repeat) {}

		bool IsRepeat() const { return m_repeat; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << m_keyCode << " (repeat = " << m_repeat << ")";
			return ss.str();
		}

		EVENT_CLASS_TYPE(KeyPressed)
	private:
		bool m_repeat;
	};

	class KeyReleasedEvent : public KeyEvent {
	public:
        explicit KeyReleasedEvent(const KeyCode keycode): KeyEvent(keycode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_keyCode;
			return ss.str();
		}

		EVENT_CLASS_TYPE(KeyReleased)
	};

	class KeyTypedEvent : public KeyEvent {
	public:
        explicit KeyTypedEvent(const KeyCode keycode): KeyEvent(keycode) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "KeyTypedEvent: " << m_keyCode;
			return ss.str();
		}

		EVENT_CLASS_TYPE(KeyTyped)
	};
}
