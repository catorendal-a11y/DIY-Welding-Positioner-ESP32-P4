#pragma once
#include <lvgl.h>

// Reuse existing bitmap glyphs; only digit advances and centering change.
class UiTabularFont {
 public:
  UiTabularFont(const UiTabularFont&) = delete;
  UiTabularFont& operator=(const UiTabularFont&) = delete;
  explicit UiTabularFont(const lv_font_t* source) : source_(source), font_(*source) {
    for (uint32_t digit = '0'; digit <= '9'; ++digit) {
      lv_font_glyph_dsc_t glyph{};
      if (source->get_glyph_dsc(source, &glyph, digit, 0) && glyph.adv_w > digitWidth_)
        digitWidth_ = glyph.adv_w;
    }
    font_.user_data = this;
    font_.kerning = LV_FONT_KERNING_NONE;
    font_.get_glyph_dsc = describe;
  }

  const lv_font_t* get() const { return &font_; }

 private:
  static bool describe(const lv_font_t* font, lv_font_glyph_dsc_t* glyph,
                       uint32_t letter, uint32_t) {
    const auto* self = static_cast<const UiTabularFont*>(font->user_data);
    if (!self->source_->get_glyph_dsc(self->source_, glyph, letter, 0)) return false;
    if (letter >= '0' && letter <= '9') {
      glyph->ofs_x += (self->digitWidth_ - glyph->adv_w) / 2;
      glyph->adv_w = self->digitWidth_;
    }
    return true;
  }

  const lv_font_t* source_;
  lv_font_t font_;
  uint16_t digitWidth_ = 0;
};

inline const lv_font_t* ui_readout_font() {
  static UiTabularFont font(&lv_font_montserrat_28);
  return font.get();
}
