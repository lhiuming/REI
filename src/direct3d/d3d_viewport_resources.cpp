#if DIRECT3D_ENABLED
#include "d3d_viewport_resources.h"

#include <memory>

#include "../common.h"

using std::runtime_error;
using std::shared_ptr;
using std::make_shared;

namespace rei {

namespace d3d {

constexpr static const WCHAR* c_rt_buffer_names[8] = {
    L"m_rt_buffers[0]",
    L"m_rt_buffers[1]",
    L"m_rt_buffers[2]",
    L"m_rt_buffers[3]",
    L"m_rt_buffers[4]",
    L"m_rt_buffers[5]",
    L"m_rt_buffers[6]",
    L"m_rt_buffers[7]",
};

constexpr static const WCHAR* c_ds_buffer_name = L"m_ds_buffer";

ViewportResources::ViewportResources(shared_ptr<DeviceResources> dev_res, HWND hwnd, int init_width,
  int init_height, v_array<std::shared_ptr<DefaultBufferData>, 4> rt_buffers,
  std::shared_ptr<DefaultBufferData> ds_buffer)
    : m_device_resources(dev_res),
      hwnd(hwnd),
      width(init_width),
      height(init_height),
      swapchain_buffer_count(rt_buffers.size()),
      m_rt_buffers(rt_buffers),
      m_ds_buffer(ds_buffer) {
  REI_ASSERT(m_device_resources);
  REI_ASSERT(hwnd);
  REI_ASSERT((width > 0) && (height > 0));

  auto device = dev_res->device();

  HRESULT hr;

  // Create rtv and dsv heaps
  D3D12_DESCRIPTOR_HEAP_DESC rtv_heap_desc;
  rtv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtv_heap_desc.NumDescriptors = swapchain_buffer_count;
  rtv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // TODO check this
  rtv_heap_desc.NodeMask = 0;                            // TODO check this
  hr = device->CreateDescriptorHeap(&rtv_heap_desc, IID_PPV_ARGS(&m_rtv_heap));
  REI_ASSERT(SUCCEEDED(hr));

  current_back_buffer_index = 0;
  rtv_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  D3D12_DESCRIPTOR_HEAP_DESC dsv_heap_desc;
  dsv_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
  dsv_heap_desc.NumDescriptors = 1; // afterall we need only one DS buffer
  dsv_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dsv_heap_desc.NodeMask = 0;
  hr = device->CreateDescriptorHeap(&dsv_heap_desc, IID_PPV_ARGS(&m_dsv_heap));
  REI_ASSERT(SUCCEEDED(hr));

  // Some size-dependent resources
  create_size_dependent_resources();
}

ViewportResources::~ViewportResources() {
  // default behaviour
}

ID3D12Resource* ViewportResources::get_rt_buffer(UINT index) const {
  REI_ASSERT(index <= swapchain_buffer_count);
  ComPtr<ID3D12Resource> ret;
  REI_ASSERT(SUCCEEDED(m_swapchain->GetBuffer(index, IID_PPV_ARGS(&ret))));
  return ret.Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE ViewportResources::get_rtv(UINT index) const {
  return CD3DX12_CPU_DESCRIPTOR_HANDLE(
    m_rtv_heap->GetCPUDescriptorHandleForHeapStart(), index, rtv_descriptor_size);
}

D3D12_CPU_DESCRIPTOR_HANDLE ViewportResources::get_dsv() const {
  return m_dsv_heap->GetCPUDescriptorHandleForHeapStart();
}

void ViewportResources::create_size_dependent_resources() {
  HRESULT hr;

  auto device = this->device();
  auto dxgi_factory = this->dxgi_factory();

  // Create Swapchain and render-target-views
  {
    /*
     * NOTE: In DX12, IDXGIDevice is not available anymore, so IDXGIFactory/IDXGIAdaptor has to be
     * created/specified, rather than being found through IDXGIDevice.
     */
    ComPtr<IDXGISwapChain1> base_swapchain;
    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullscreen_desc = {}; // use it if you need fullscreen
    fullscreen_desc.RefreshRate.Numerator = 60;
    fullscreen_desc.RefreshRate.Denominator = 1;
    fullscreen_desc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    fullscreen_desc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    DXGI_SWAP_CHAIN_DESC1 chain_desc = {};
    chain_desc.Width = width;
    chain_desc.Height = height;
    chain_desc.Format = m_target_spec.dxgi_rt_format;
    chain_desc.Stereo = FALSE;
    chain_desc.SampleDesc = m_target_spec.sample_desc;
    chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain_desc.BufferCount = swapchain_buffer_count; // total frame buffer count
    chain_desc.Scaling = DXGI_SCALING_STRETCH;
    chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // TODO DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL
    chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    chain_desc.Flags = 0;
    hr = dxgi_factory->CreateSwapChainForHwnd(this->command_queue(), hwnd, &chain_desc,
      NULL, // windowed app
      NULL, // optional
      &base_swapchain);
    REI_ASSERT(SUCCEEDED(hr));

    hr = base_swapchain.As(&m_swapchain);
    REI_ASSERT(SUCCEEDED(hr));

    DefaultBufferFormat meta;
    meta.dimension = ResourceDimension::Texture2D;
    meta.format = m_target_spec.rt_format;
    D3D12_RENDER_TARGET_VIEW_DESC* default_rtv_desc = nullptr; // default initialization
    for (UINT i = 0; i < swapchain_buffer_count; i++) {
      hr = m_swapchain->GetBuffer(i, IID_PPV_ARGS(&m_rt_buffers[i]->buffer));
#if DEBUG
      REI_ASSERT(SUCCEEDED(m_rt_buffers[i]->buffer->SetName(c_rt_buffer_names[i])));
#endif
      m_rt_buffers[i]->state = D3D12_RESOURCE_STATE_PRESENT;
      m_rt_buffers[i]->meta = meta;
      REI_ASSERT(SUCCEEDED(hr));
      device->CreateRenderTargetView(m_rt_buffers[i]->buffer.Get(), default_rtv_desc, get_rtv(i));
    }

  }

  // Create depth stencil buffer and view
  {
    // TODO simplify this using d3dx12.h helpers
    D3D12_HEAP_PROPERTIES ds_heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_HEAP_FLAGS ds_heap_flags = D3D12_HEAP_FLAG_NONE; // default
    D3D12_RESOURCE_DESC ds_desc = {};
    ds_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    ds_desc.Alignment = 0; // default; let the runtime choose
    ds_desc.Width = width;
    ds_desc.Height = height;
    ds_desc.DepthOrArraySize = 1;             // just a normal texture
    ds_desc.MipLevels = 1;                    // see above
    ds_desc.Format = m_target_spec.dxgi_ds_format; // srandard choice
    ds_desc.SampleDesc = m_target_spec.sample_desc;
    ds_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;                       // defualt; TODO check this
    ds_desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;             // as we intent
    D3D12_RESOURCE_STATES init_state = D3D12_RESOURCE_STATE_DEPTH_WRITE; // alway this state
    D3D12_CLEAR_VALUE optimized_clear = {}; // special clear value; usefull for framebuffer types
    optimized_clear.Format = m_target_spec.dxgi_ds_format;
    optimized_clear.DepthStencil = m_target_spec.ds_clear;
    hr = device->CreateCommittedResource(&ds_heap_prop, ds_heap_flags, &ds_desc, init_state,
      &optimized_clear, IID_PPV_ARGS(&m_ds_buffer->buffer));
    REI_ASSERT(SUCCEEDED(hr));

    #if DEBUG
    REI_ASSERT(SUCCEEDED(m_ds_buffer->buffer->SetName(c_ds_buffer_name)));
    #endif

    DefaultBufferFormat meta;
    meta.dimension = ResourceDimension::Texture2D;
    meta.format = m_target_spec.ds_format;

    m_ds_buffer->meta = meta;
    m_ds_buffer->state = init_state;

    D3D12_DEPTH_STENCIL_VIEW_DESC* p_dsv_desc = nullptr; // default value: same format as buffer[0]
    device->CreateDepthStencilView(m_ds_buffer->buffer.Get(), p_dsv_desc, get_dsv());
  }

  // update to current "current" index
  current_back_buffer_index = m_swapchain->GetCurrentBackBufferIndex();
 }

void ViewportResources::update_size(int width, int height) {
  this->width = width;
  this->height = height;
  create_size_dependent_resources();
}

} // namespace d3d

} // namespace rei

#endif
