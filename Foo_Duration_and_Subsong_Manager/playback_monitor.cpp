#include "stdafx.h"
#include "global_container.h"
#include "duration_database.h"
namespace {

    static const UINT_PTR TIMER_ID = 0xD5B1;
    static const UINT TIMER_INTERVAL_MS = 50;
    

    class duration_playback_monitor : public play_callback_static {
    private:
        double m_stop_at = -1;
        bool m_timer_active = false;
        HWND m_hwnd = nullptr;
    public:
        unsigned get_flags() override {
            return flag_on_playback_new_track | flag_on_playback_stop | flag_on_playback_pause;
        }

        void on_playback_new_track(metadb_handle_ptr p_track) override {
            pfc::string8 hash_key = duration_db::content_hasher(p_track->get_path(), p_track->get_subsong_index());
            if (song_container.contains(hash_key) && (song_container[hash_key].custom_duration != song_container[hash_key].original_duration)) {
                m_stop_at = song_container[hash_key].custom_duration;
                start_timer();
            }
            else {
                m_stop_at = -1;
                stop_timer();
            }
        }

        void on_playback_stop(play_control::t_stop_reason) override {
            m_stop_at = -1;
            stop_timer();
        }

        void on_playback_pause(bool p_state) override {
            if (m_stop_at <= 0) return;
            if (p_state) {
                stop_timer();
            }
            else {
                start_timer();
            }
        }

        void on_playback_starting(play_control::t_track_command, bool) override {}
        void on_playback_seek(double) override {}
        void on_playback_time(double) override {}
        void on_playback_edited(metadb_handle_ptr) override {}
        void on_playback_dynamic_info(const file_info&) override {}
        void on_playback_dynamic_info_track(const file_info&) override {}
        void on_volume_change(float) override {}

    private:
        void start_timer() {
            if (m_timer_active) return;
            if (!create_message_window()) return;
            ::SetTimer(m_hwnd, TIMER_ID, TIMER_INTERVAL_MS, nullptr);
            m_timer_active = true;
        }

        void stop_timer() {
            if (!m_timer_active) return;
            ::KillTimer(m_hwnd, TIMER_ID);
            m_timer_active = false;
        }
        //Create a message-only window (HWND_MESSAGE) to monitor playback by handling WM_TIMER
        // 用 HWND_MESSAGE 创建「仅消息窗口」，通过WM_TIMER实现时长监视
        bool create_message_window() {
            if (m_hwnd) return true;

            static const wchar_t* CLASS_NAME = L"DurationMonitorMsgWnd";
            static bool registered = false;
            if (!registered) {
                WNDCLASSW wc = {};
                wc.lpfnWndProc = WndProc;
                wc.hInstance = core_api::get_my_instance();
                wc.lpszClassName = CLASS_NAME;
                ::RegisterClassW(&wc);
                registered = true;
            }
            m_hwnd = ::CreateWindowW(CLASS_NAME, L"", 0, 0, 0, 0, 0,
                HWND_MESSAGE, nullptr, core_api::get_my_instance(), this);
            return m_hwnd != nullptr;
        }
        // 创建时把 this 指针（CreateWindow 传入的 lpCreateParams）存进 GWLP_USERDATA，
        // 之后静态的 WndProc 就能取回它并回调到具体对象实例。
        // On WM_CREATE, stash the `this` pointer (passed as lpCreateParams) into GWLP_USERDATA,
        // so this static WndProc can retrieve it later and dispatch to the object instance。
        static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
            
            if (msg == WM_CREATE) {
                auto cs = reinterpret_cast<CREATESTRUCT*>(lp);
                ::SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
                return 0;
            }
            if (msg == WM_TIMER && wp == TIMER_ID) {
                auto self = reinterpret_cast<duration_playback_monitor*>(
                    ::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
                if (self) self->on_timer();
                return 0;
            }
            return ::DefWindowProcW(hwnd, msg, wp, lp);
        }

        // 定时轮询播放位置：一旦到达自定义时长 m_stop_at 就切到下一首——这就是「自定义时长截断播放」实际生效的地方。
        // Polls the playback position: once it reaches the custom length m_stop_at, jump to the next track
        // This is where "custom-duration truncation" actually takes effect.
        void on_timer() {
            if (m_stop_at <= 0) return;
            auto pc = playback_control::get();
            if (!pc->is_playing() || pc->is_paused()) return;

            double pos = pc->playback_get_position();
            if (pos >= m_stop_at) {
                m_stop_at = -1;
                stop_timer();
                pc->start(playback_control::track_command_next);
            }
        }
    };

    static play_callback_static_factory_t<duration_playback_monitor> g_playback_monitor;

} // namespace