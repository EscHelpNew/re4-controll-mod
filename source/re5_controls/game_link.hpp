#pragma once
#include <switch.h>
#include "re5/settings.hpp"
#include "re5/shared.hpp"
#include "re5/profile.hpp"
#include "re5/camera_probe.hpp"
#include <string>
#include <array>
#include <cstddef>

class Re5GameLink {
    Service service_{};
    u64 process_=0,address_=0;
    u32 expected_sequence_=0;
    re5::profile::Build connected_build_=re5::profile::Build::None;
    u64 connected_main_base_=0,connected_main_size_=0,connected_control_bytes_=0;
    bool recovered_identity_=false;
    bool info_ready_=false,initialized_=false,borrowed_=false;
    Result info_result_=0,service_result_=0;
    struct DiagnosticResult {Result value=0;bool called=false;};
    struct DiagnosticState {
        u64 process_id=0,program_id=0,metadata_process_id=0,metadata_program_id=0;
        u64 main_base=0,main_size=0;
        std::array<u8,32> build_id{},effective_build_id{};
        const char* identity_source="unavailable";
        std::array<char,600> image_notes{};
        bool process_valid=false,program_valid=false,metadata_valid=false,supported=false;
        const char* status="not_connected";
        // Process, program, attach, metadata, then process/program verification.
        std::array<DiagnosticResult,6> results{};
    } observed_;
    // Each observer owns its scratch; production UI and worker objects live in BSS.
    // Keep the exported5808-byte wire and large JSON buffers off the worker stack.
    re5::CameraProbe camera_probe_buffer_{};
    std::array<char,8192> camera_probe_json_{};
    std::array<char,6144> diagnostic_routes_{};
    std::array<char,16384> diagnostic_json_{};
    const char* tr(const char* en,const char* ru)const{return language==re4::Language::Russian?ru:en;}
public:
    re4::Language language=re4::Language::English;
    std::string details,message="Not connected";
    re5::SharedControl diagnostics{};
    void initialize();
    // Reuse owner's dmnt:cht session: a second smGetService can block at the session limit.
    // Owner must outlive this instance; close the borrower before closing its owner.
    void initialize_shared(const Re5GameLink& owner);
    void close();
    bool connect();
    // Refreshes read-only metadata/IPC. Returns JSON bytes excluding NUL, or 0 with empty output.
    std::size_t diagnostic_snapshot(char* out,std::size_t capacity);
    bool apply(const re5::Settings& settings);
};
