#ifdef __spirv__
#	define VK(x) [[vk::x]]
#else
#	define VK(x)
#endif

struct RootConstants
{
	float angle;
};

#ifdef __spirv__
VK(push_constant) RootConstants g_root;
#else
ConstantBuffer<RootConstants> g_root : register(b0, space0);
#endif

struct VS_INPUT
{
	float3 pos : VINPUT0;
	float3 color : VINPUT1;
	float2 texcoord : VINPUT2;
};

struct PS_INPUT
{
	float4 position : SV_Position;
	float3 color : COLOR0;
	float2 texcoord : TEXCOORD0;
};

PS_INPUT Vertex(VS_INPUT input)
{
	PS_INPUT output;
	float angle = g_root.angle;
	float2x2 rotation = {
		cos(angle), sin(angle),
		-sin(angle), cos(angle),
	};

	output.position = float4(mul(rotation, input.pos.xy), input.pos.z, 1.0);
	output.color = input.color;
	output.texcoord = input.texcoord;

	return output;
}

VK(binding(0)) Texture2D<float4> g_image : register(t0, space0);
VK(binding(1)) SamplerState g_sampler : register(s0, space0);

float4 Pixel(PS_INPUT input) : SV_Target
{
	return g_image.Sample(g_sampler, input.texcoord) * float4(input.color, 1.0);
}
