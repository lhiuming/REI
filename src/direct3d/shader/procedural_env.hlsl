#ifndef REI_PROCUDURAL_HLSL
#define REI_PROCUDURAL_HLSL

// Mimicing the backgroud coloring in eva 2020 poster
float3 gradiant_sky_eva(float3 view_direction) {
  float3 ayanami_blue = {129.0 / 255, 187.0 / 255, 235.0 / 255};
  float3 asuka_red = {156.0 / 255, 0, 0};
  float y = 1 - view_direction.y;
  float mixing = saturate((y + 0.25) / 2);
  mixing = pow(mixing, 5);
  return lerp(ayanami_blue, asuka_red, mixing);
}

#endif