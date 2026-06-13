#include"stdafx.h"
#include <string>
#include <libPPUI/win32_op.h>
#include <helpers/BumpableElem.h>
#include "GUID.h"


namespace {
	// Anonymous namespace : standard practice in fb2k components
	// Nothing outside should have any reason to see these symbols, and we don't want funny results if another cpp has similarly named classes.
	// service_factory at the bottom takes care of publishing our class.

	class CHpTimerWindow : public ui_element_instance /*foobar2000SDK_UI Interface*/, 
						  public CWindowImpl<CHpTimerWindow>/*WindowsATL Interface*/, 
		                  public play_callback_impl_base/*foobar2000SDK_playback Interface*/ {
	public:
		// ATL window registration and initialization
		DECLARE_WND_CLASS_EX(TEXT("{87F2D517-2CAD-4EA0-B5B9-82B3DC287977}"), CS_VREDRAW | CS_HREDRAW, (-1));
		void initialize_window(HWND parent) { WIN32_OP(Create(parent) != NULL); }
		
		BEGIN_MSG_MAP_EX(CHpTimerWindow)
			MSG_WM_CREATE(OnCreate)
			MSG_WM_ERASEBKGND(OnEraseBkgnd)
			MSG_WM_TIMER(OnTimer)
			MSG_WM_PAINT(OnPaint)
		END_MSG_MAP()
		CHpTimerWindow(ui_element_config::ptr, ui_element_instance_callback_ptr p_callback);
		HWND get_wnd() { return *this; }
		void set_configuration(ui_element_config::ptr config) { m_config = config; }
		ui_element_config::ptr get_configuration() { return m_config; }
		static GUID g_get_guid() {
			return guid_timer_ui;
		}
		static GUID g_get_subclass() { return ui_element_subclass_utility; }
		static void g_get_name(pfc::string_base& out) { out = "High-Precision Timer"; }
		static ui_element_config::ptr g_get_default_configuration() { return ui_element_config::g_create_empty(g_get_guid()); }
		static const char* g_get_description() { return "Shows and updates playback time and track length in ms([HH:]MM:SS.mmm)"; }

		void notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size);
	private:
		int OnCreate(LPCREATESTRUCT);
		void OnDestroy();
		void OnTimer(UINT_PTR id);
		void ContentRenderer(pfc::string_base& out);
		void TextFormatter(pfc::string_base& out, double pos, double len = 0);
		void OnPaint(CDCHandle);

		BOOL OnEraseBkgnd(CDCHandle);
		ui_element_config::ptr m_config;
		//Handlers for playback states change
		void on_playback_starting(play_control::t_track_command p_command, bool p_pausesd) override {
			if (!p_pausesd) sync_timer_to_playback();
		}
		void on_playback_new_track(metadb_handle_ptr p_track) override { sync_timer_to_playback(); }
		void on_playback_stop(play_control::t_stop_reason p_reason) override {
			stop_timer();
			Invalidate();
		}
		void on_playback_seek(double p_time) override { Invalidate(); }
		void on_playback_pause(bool paused) override {
			if (paused) {
				stop_timer();
				Invalidate();
			}
			else {
				start_timer();
			}
		}
		void on_playback_edited(metadb_handle_ptr p_track) override { Invalidate(); }
		void on_playback_dynamic_info_track(const file_info& p_info) override { Invalidate(); }
		//Timer Helpers
		int const TIMER_ID = 1;
		int const TIMER_MS = 16;
		BOOL is_timer_on = false;
		void sync_timer_to_playback() {
			auto pc = playback_control::get();
			if (pc->is_playing() && !pc->is_paused()) {
				start_timer();
			}
			else {
				stop_timer();
			}
			Invalidate();
		}
		void start_timer() {
			if (!is_timer_on) {
				SetTimer(TIMER_ID, TIMER_MS);
				is_timer_on = true;
			}
		}
		void stop_timer() {
			if (is_timer_on) {
				KillTimer(TIMER_ID);
				is_timer_on = false;
			}
		}
	protected:
		// this must be declared as protected for ui_element_impl_withpopup<> to work.
		const ui_element_instance_callback_ptr m_callback;
	};
	void CHpTimerWindow::notify(const GUID& p_what, t_size p_param1, const void* p_param2, t_size p_param2size) {
		if (p_what == ui_element_notify_colors_changed || p_what == ui_element_notify_font_changed) {
			// we use global colors and fonts - trigger a repaint whenever these change.
			Invalidate();
		}
	}
	CHpTimerWindow::CHpTimerWindow(ui_element_config::ptr config, ui_element_instance_callback_ptr p_callback) : m_callback(p_callback), m_config(config) {
	}
	//创建窗口时将定时器同步到正在播放的音轨
	//Sync timer to the playing track
	int CHpTimerWindow::OnCreate(LPCREATESTRUCT) {
		sync_timer_to_playback();
		return 0;
	}
	//销毁窗口时停止计时器
	//Stop timer when the window is destroyed
	void CHpTimerWindow::OnDestroy() {
		stop_timer();
	}
	//禁用背景擦除
	//Disable Background Erase
	BOOL CHpTimerWindow::OnEraseBkgnd(CDCHandle) {
		return TRUE;
	}
	//绘制函数一并处理背景+内容，并且先在内存中绘制，画好后传给屏幕。双缓冲机制最大程度防止闪烁
	//Paint background and content together and use memory buffer to avoid blink as much as possible
	void CHpTimerWindow::OnPaint(CDCHandle) {
		CPaintDC dc(*this);
		CRect rc;
		WIN32_OP_D(GetClientRect(&rc));
		CDC mem;
		mem.CreateCompatibleDC(dc);
		CBitmap bmp;
		bmp.CreateCompatibleBitmap(dc, rc.Width(), rc.Height());
		HBITMAP old_bmp = mem.SelectBitmap(bmp);
		COLORREF bg = m_callback->query_std_color(ui_color_background);
		CBrush bg_brush;
		bg_brush.CreateSolidBrush(bg);
		mem.FillRect(&rc, bg_brush);
		mem.SetTextColor(m_callback->query_std_color(ui_color_text));
		mem.SetBkMode(TRANSPARENT);
		HFONT font = (HFONT)m_callback->query_font_ex(ui_font_default);
		HFONT old_font = mem.SelectFont(font);
		const UINT format = DT_NOPREFIX | DT_LEFT | DT_VCENTER | DT_SINGLELINE;
		pfc::string8 text;
		ContentRenderer(text);
		pfc::stringcvt::string_wide_from_utf8 wtext(text);
		CRect text_rc = rc;
		text_rc.DeflateRect(4, 0);
		WIN32_OP_D(mem.DrawText(wtext, -1, &text_rc, format) > 0);
		mem.SelectFont(old_font);
		dc.BitBlt(0, 0, rc.Width(), rc.Height(), mem, 0, 0, SRCCOPY);
		mem.SelectBitmap(old_bmp);
	}
	
	void CHpTimerWindow::OnTimer(UINT_PTR id) {
		if (id == TIMER_ID) Invalidate();
	}
	
	void CHpTimerWindow::ContentRenderer(pfc::string_base& out) {
		auto pc = playback_control::get();
		if (pc->is_playing()) {
			if (!pc->playback_get_length_ex() && !pc->playback_get_position()) {
				out << "Opening...";
			}
			else if (!pc->playback_get_length_ex() && pc->playback_get_position()) {
				TextFormatter(out, pc->playback_get_position());
			}
			else {
				TextFormatter(out, pc->playback_get_position(), pc->playback_get_length_ex());
			}
		}
	}
	void CHpTimerWindow::TextFormatter(pfc::string_base& out, double pos, double len) {
		if (len) {
			out << pfc::format_time_ex(pos, 3) << " / " << pfc::format_time_ex(len, 3);
		}
		else {
			out << pfc::format_time_ex(pos, 3);
		}
		
	}
	// ui_element_impl_withpopup autogenerates standalone version of our component and proper menu commands. Use ui_element_impl instead if you don't want that.
	class ui_element_myimpl : public ui_element_impl_withpopup<CHpTimerWindow> {
	public:
		//自定义窗口初始大小
		//Custom window size as a very small rectangle
		bool get_popup_specs(ui_size& defSize, pfc::string_base& title) override {
			defSize = { 220,24 };
			title = "High-Precision Timer";
			return true;
		}
	};

	static service_factory_single_t<ui_element_myimpl> g_ui_element_myimpl_factory;

} // namespace

