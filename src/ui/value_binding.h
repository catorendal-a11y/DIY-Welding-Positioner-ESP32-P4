#pragma once
#include <lvgl.h>
#include <cstring>

// UI-thread-only, one widget per binding. Reset before rebuilding its screen.
// Objects unsubscribe automatically; our delete hook also protects the fallback
// path if LVGL cannot allocate a subject or observer.
class UiBinding {
 public:
  UiBinding() = default;
  UiBinding(const UiBinding&) = delete;
  UiBinding& operator=(const UiBinding&) = delete;
  void reset() {
    if (widget_) lv_obj_remove_event_cb_with_user_data(widget_, deleted, this);
    lv_subject_delete(subject_);
    subject_ = nullptr;
    widget_ = nullptr;
  }
 protected:
  lv_subject_t* subject_ = nullptr;
  lv_obj_t* widget_ = nullptr;
  void attach(lv_obj_t* widget, lv_subject_type_t type) {
    reset();
    widget_ = widget;
    if (!widget_) return;
    lv_obj_add_event_cb(widget_, deleted, LV_EVENT_DELETE, this);
    subject_ = lv_subject_create(type);
  }
  bool finish(lv_observer_t* observer) {
    if (observer) return true;
    lv_subject_delete(subject_);
    subject_ = nullptr;
    return false;
  }
 private:
  static void deleted(lv_event_t* event) {
    static_cast<UiBinding*>(lv_event_get_user_data(event))->widget_ = nullptr;
  }
};

template <size_t Capacity> class UiTextBinding : public UiBinding {
 public:
  bool bind(lv_obj_t* label) {
    attach(label, LV_SUBJECT_TYPE_STRING);
    if (!subject_) return false;
    lv_subject_set_string_buffer_static(subject_, buffer_, nullptr, Capacity);
    lv_subject_set_string(subject_, lv_label_get_text(label));
    return finish(lv_obj_bind_string(label, subject_, lv_label_set_text));
  }
  void set(const char* text) {
    if (!widget_) return;
    if (subject_) {
      // Without a previous-string buffer LVGL notifies on every set. Compare
      // here so unchanged labels do not allocate text on every UI tick.
      if (std::strcmp(buffer_, text) != 0) lv_subject_set_string(subject_, text);
    } else if (std::strcmp(lv_label_get_text(widget_), text) != 0) {
      lv_label_set_text(widget_, text);
    }
  }
 private:
  static_assert(Capacity > 0, "Text bindings need a terminator");
  char buffer_[Capacity]{};
};

class UiIntBinding : public UiBinding {
 public:
  bool bind(lv_obj_t* widget, lv_obj_set_int_t setter) {
    setter_ = setter;
    attach(widget, LV_SUBJECT_TYPE_INT);
    return subject_ && finish(lv_obj_bind_int(widget, subject_, setter));
  }
  void set(int32_t value) {
    if (!widget_) return;
    if (subject_) lv_subject_set_int(subject_, value);
    else if (setter_) setter_(widget_, value);
  }
 private:
  lv_obj_set_int_t setter_ = nullptr;
};

class UiBoolBinding : public UiBinding {
 public:
  bool bind(lv_obj_t* widget, lv_obj_set_bool_t setter, bool initial = false) {
    setter_ = setter;
    attach(widget, LV_SUBJECT_TYPE_INT);
    if (!subject_) { set(initial); return false; }
    lv_subject_set_int(subject_, initial);
    const bool bound = finish(lv_obj_bind_bool(widget, subject_, setter));
    if (!bound) set(initial);
    return bound;
  }
  void set(bool value) {
    if (!widget_) return;
    if (subject_) lv_subject_set_int(subject_, value);
    else if (setter_) setter_(widget_, value);
  }
 private:
  lv_obj_set_bool_t setter_ = nullptr;
};
