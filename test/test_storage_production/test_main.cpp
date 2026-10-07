#include <unity.h>
#include <stdexcept>
#include <new>
#include "../../src/storage/storage.cpp"
SimSerial Serial; SimEsp ESP;
std::atomic<uint32_t> g_inputHeartbeatMs{0};
std::atomic<bool> g_restartRequired{false};
std::atomic<bool> g_wakePending{false},g_dir_switch_cache{false},g_flashWriting{false},g_screenRedraw{false};
[[noreturn]] void fatal_halt(const char* reason) { throw std::runtime_error(reason); }
void event_log_addf(const char*,...) {}
SystemState control_get_state() { return STATE_IDLE; }
bool control_stop() { return true; }
void setUp() {
  if(!g_nvs_mutex) g_nvs_mutex=xSemaphoreCreateMutex();
  if(!g_settings_mutex) g_settings_mutex=xSemaphoreCreateMutex();
  if(!g_presets_mutex) g_presets_mutex=xSemaphoreCreateMutex();
  testNvs.clear(); testLegacy.clear(); testNvsShortWrite=testNvsShortRead=testNvsEraseFailure=false;
  testNvsBeforeClear = nullptr; g_restartRequired.store(false);
  g_prefs_open=true; g_presets.clear(); g_settings.max_rpm=MAX_RPM;
  settingsSave.~SaveRequest(); new (&settingsSave) SaveRequest{1000};
  presetsSave.~SaveRequest(); new (&presetsSave) SaveRequest{500}; simTestMillis=100;
}
void tearDown() {}
static void blob(const char* key,const char* text) {
  testNvs[key]=std::vector<uint8_t>(text,text+strlen(text));
}
void test_storage_real_roundtrip_preserves_legacy_utf8_names() {
  Preset p{}; p.id=1; strcpy(p.name,"N\xc3\xb8r"); p.rpm=0.5f; p.step_angle=90; p.step_repeats=1;
  p.mode=STATE_RUNNING; p.mode_mask=PRESET_MASK_CONT;
  g_presets.push_back(p); TEST_ASSERT_TRUE(storage_save_presets_internal());
  g_presets.clear(); TEST_ASSERT_TRUE(storage_load_presets());
  TEST_ASSERT_EQUAL_STRING(p.name,g_presets[0].name);
}
void test_storage_malformed_objects_never_replace_live_programs() {
  Preset p{}; strcpy(p.name,"Original"); g_presets.push_back(p);
  for(const char* text : {"[42]","[null]","{}","[{\"name\":\"bad\\nname\"}]"}) {
    blob("prs",text); TEST_ASSERT_FALSE(storage_load_presets());
    TEST_ASSERT_EQUAL_STRING("Original",g_presets[0].name);
  }
}
void test_storage_normalizes_duplicate_and_missing_ids() {
  blob("prs","[{\"id\":255,\"name\":\"One\"},{\"id\":255,\"name\":\"Two\"},{\"name\":\"Three\"}]");
  TEST_ASSERT_TRUE(storage_load_presets());
  for(size_t i=0;i<g_presets.size();++i) TEST_ASSERT_EQUAL(i+1,g_presets[i].id);
}
void test_storage_bounds_blob_and_requires_exact_read_write() {
  testNvs["prs"].resize(PRESETS_BLOB_MAX+1); TEST_ASSERT_FALSE(storage_load_presets());
  testNvs["cfg"].resize(SETTINGS_BLOB_MAX+1); TEST_ASSERT_FALSE(storage_load_settings());
  blob("cfg","{}"); testNvsShortRead=true; TEST_ASSERT_FALSE(storage_load_settings());
  testNvsShortRead=false; testNvsShortWrite=true;
  TEST_ASSERT_FALSE(storage_save_settings_internal()); TEST_ASSERT_FALSE(storage_save_presets_internal());
}
void test_storage_migration_validates_both_before_either_write() {
  testLegacy[SETTINGS_FILE]={'{','}'}; testLegacy[PRESETS_FILE]={'[','4','2',']'};
  bool rejected=false; try { storage_migrate_littlefs_to_nvs(); } catch(const std::runtime_error&) { rejected=true; }
  TEST_ASSERT_TRUE(rejected); TEST_ASSERT_EQUAL(0,g_prefs.getBytesLength("cfg"));
  TEST_ASSERT_EQUAL(2,testLegacy.size());
}
void test_storage_preset_receipt_is_not_success_before_exact_commit() {
  const uint32_t ticket=storage_request_presets_save();
  TEST_ASSERT_EQUAL(STORAGE_PENDING,storage_presets_save_status(ticket));
  simTestMillis+=500; testNvsShortWrite=true; storage_flush();
  TEST_ASSERT_EQUAL(STORAGE_ERROR,storage_presets_save_status(ticket));
  testNvsShortWrite=false; simTestMillis+=30000; storage_flush();
  TEST_ASSERT_EQUAL(STORAGE_SAVED,storage_presets_save_status(ticket));
}
void test_storage_erase_failure_preserves_ram() {
  Preset p{}; strcpy(p.name,"Retained"); g_presets.push_back(p); testNvsEraseFailure=true;
  TEST_ASSERT_FALSE(storage_format()); TEST_ASSERT_EQUAL(1,g_presets.size());
  TEST_ASSERT_FALSE(g_restartRequired.load());
}
void test_format_inhibits_motion_before_entering_flash_erase() {
  testNvsBeforeClear = [] { TEST_ASSERT_TRUE(g_restartRequired.load()); TEST_ASSERT_EQUAL(HIGH, simTestPins[PIN_ENA]); };
  TEST_ASSERT_TRUE(storage_format()); TEST_ASSERT_TRUE(g_restartRequired.load());
}
void test_name_policy_rejects_split_overlong_utf8_and_controls() {
  TEST_ASSERT_TRUE(program_name_valid("N\xc3\xb8r",32));
  TEST_ASSERT_FALSE(program_name_valid("N\xc3\xb8r",32,true));
  TEST_ASSERT_TRUE(program_name_valid("Weld 01 / test",32,true));
  for(const char* name : {"bad\nname","\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xc3"})
    TEST_ASSERT_FALSE(program_name_valid(name,32));
  TEST_ASSERT_FALSE(program_name_valid("12345678901234567890123456789012",32));
}

void test_storage_overflowed_document_never_reaches_nvs() {
  struct RejectAllocator : ArduinoJson::Allocator {
    void* allocate(size_t) override { return nullptr; }
    void deallocate(void*) override {}
    void* reallocate(void*,size_t) override { return nullptr; }
  } allocator;
  JsonDocument doc(&allocator); doc["name"]="Incomplete";
  TEST_ASSERT_TRUE(doc.overflowed());
  TEST_ASSERT_FALSE(storage_write_doc("prs",doc,PRESETS_BLOB_MAX));
  TEST_ASSERT_EQUAL(0,g_prefs.getBytesLength("prs"));
}

int main() {
 UNITY_BEGIN();
 RUN_TEST(test_storage_overflowed_document_never_reaches_nvs);
 RUN_TEST(test_storage_real_roundtrip_preserves_legacy_utf8_names);
 RUN_TEST(test_storage_malformed_objects_never_replace_live_programs);
 RUN_TEST(test_storage_normalizes_duplicate_and_missing_ids);
 RUN_TEST(test_storage_bounds_blob_and_requires_exact_read_write);
 RUN_TEST(test_storage_migration_validates_both_before_either_write);
 RUN_TEST(test_storage_preset_receipt_is_not_success_before_exact_commit);
 RUN_TEST(test_storage_erase_failure_preserves_ram);
 RUN_TEST(test_format_inhibits_motion_before_entering_flash_erase);
 RUN_TEST(test_name_policy_rejects_split_overlong_utf8_and_controls);
 return UNITY_END();
}
