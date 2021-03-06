#ifndef REI_DIRECT3D_D3D_VIEWER_H
#define REI_DIRECT3D_D3D_VIEWER_H

#include <cstddef>

#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

#include "../input.h"
#include "viewer.h"

/*
 * d3d_viewer.h
 * Viewer class mange a window and setting interactive input by user using
 * Direct3D API. It also configure a renderer and provide it with necessary
 * infomation to render the window.
 */

namespace rei {

class WinViewer : public Viewer {
public:
  WinViewer() = delete;
  WinViewer(HINSTANCE h_instance, std::size_t window_w, std::size_t window_h, std::wstring title);
  ~WinViewer();

  SystemWindowID get_window_id() const override { return WindowID(hwnd); }

  bool is_destroyed() const override { return m_is_destroyed; }

  void update_title(const std::wstring& title) override;

private:
  struct WindowID : SystemWindowID {
    WindowID(HWND hwnd) : SystemWindowID {Win, hwnd} {}
  };

  // Configuration
  bool m_post_quit_on_destroy_msg = true;

  // Window states
  bool m_is_destroyed = false;

  // Windows interface object
  const HINSTANCE h_instance = NULL;
  LPCWSTR WndClassName = L"rei_Viewer_Window";
  HWND hwnd = NULL;

  // Some input state
  bool mouse_in_window = false;
  POINTS last_mouse_pos_regular;

  void initialize_window(HINSTANCE hInstance, int ShowWnd, int width, int height, bool windowed);
  void on_window_destroy();

  // Converto to basis with origin at Left-Bottom
  inline POINTS regularize(POINTS p) { return POINTS {p.x, SHORT(m_height) - p.y}; }

  LRESULT process_wnd_msg(UINT msg, WPARAM wParam, LPARAM lParam);

  struct HwndHasher {
    size_t operator()(const HWND& hwnd) const { return (std::uintptr_t)(hwnd); }
  };

  // Map hwnd to viewer class, for procceessing msg with context
  static std::unordered_map<HWND, WinViewer*, HwndHasher> viewer_map;

  // Windows static callback; telecast the mes to viewer
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

} // namespace rei

#endif
