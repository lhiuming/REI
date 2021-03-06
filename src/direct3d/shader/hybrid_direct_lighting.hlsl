#include "common.hlsl"
#include "hybrid_common.hlsl"
#include "raster_common.hlsl"

struct Light {
  float4 pos_dir; // for dir, w is zero; for pos, w is 1.
  float4 color;   // hdr color, defined as irradiance * PI, aka lambertian response
};

// Per light data
ConstantBuffer<Light> g_light : register(b0, space0);

// G-buffers
Texture2D<float> g_depth : register(t0, space1);
Texture2D<float4> g_rt0 : register(t1, space1);
Texture2D<float4> g_rt1 : register(t2, space1);
Texture2D<float4> g_rt2 : register(t3, space1);

// Output
RWTexture2D<float4> g_color: register(u0, space1);

// Per render info
ConstantBuffer<PerRenderConstBuffer> g_render : register(b0, space1);

void output(uint2 index, float3 radiance) {
  g_color[index] += float4(radiance, 1.0);
}

// TODO make this clustered lighting
[numthreads(8, 8, 1)]
void CS(uint3 tid :SV_DispatchThreadId) {
  float z = g_depth[tid.xy];

  // skip backgroug pixel
  if (z <= EPS) { 
    return; 
  }

  // get world pos from depth
  float2 uv = tid.xy / get_screen_size(g_render);
  float4 ndc = float4((uv - 0.5) * float2(2.f, -2.f), z, 1);
  float4 w_coord = mul(get_view_proj_inv(g_render), ndc);
  float3 w_pos = w_coord.xyz / w_coord.w;

  // Decode light data
  float3 light_dir;
  float3 light_color;
  if (g_light.pos_dir.w > 0.5) { // position
    float3 diff = g_light.pos_dir.xyz - w_pos;
    light_dir = normalize(diff);
    float dist2 = dot(diff, diff);
    light_color = g_light.color.xyz / dist2;
  } else { // direction
    light_dir = g_light.pos_dir.xyz;
    light_color = g_light.color.xyz;
  }

  // Evalute BRDF for surface
  float4 rt0 = g_rt0[tid.xy];
  float4 rt1 = g_rt1[tid.xy];
  float4 rt2 = g_rt2[tid.xy];
  Surface surf = decode_gbuffer(rt0, rt1, rt2);
  BXDFSurface bxdf = to_bxdf(surf);
  float3 viewer_dir = normalize(g_render.camera_pos.xyz - w_pos);
  BrdfCosine brdf_cos = BRDF_GGX_Lambertian(bxdf, viewer_dir, light_dir);

  // TODO shadow
  float attenuation = 1.0;

  float3 reflectance = (brdf_cos.specular + brdf_cos.diffuse) * light_color * attenuation;
  output(tid.xy, reflectance);
}
