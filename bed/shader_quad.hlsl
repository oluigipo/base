struct VS_INPUT
{
	float2 pos : VINPUT0;
	float2 texcoords : VINPUT1;
	int2   texindex : VINPUT2;
	float4 color : VINPUT3;
};

struct VS_OUTPUT
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float2 texindex : TEXCOORD1;
	float4 color : TEXCOORD2;
};

cbuffer UniformBuffer0 : register(b0)
{
	float4x4 uView;
};

VS_OUTPUT Vertex(VS_INPUT input)
{
	VS_OUTPUT output;
	
	output.position = mul(uView, float4(input.pos, 0.0, 1.0));
	output.color = input.color;
	output.texcoords = input.texcoords;
	output.texindex = float2(input.texindex);
	
	return output;
}

Texture2D<float4> uTexture0 : register(t0);
Texture2D<float4> uTexture1 : register(t1);
Texture2D<float4> uTexture2 : register(t2);
Texture2D<float4> uTexture3 : register(t3);
SamplerState uSampler0 : register(s0);
SamplerState uSampler1 : register(s1);
SamplerState uSampler2 : register(s2);
SamplerState uSampler3 : register(s3);

float4 Pixel(VS_OUTPUT input) : SV_TARGET
{
	int texindex = int(input.texindex.x);
	int texkind = int(input.texindex.y);
	float4 color;
	if (texindex == 0)
		color = uTexture0.Sample(uSampler0, input.texcoords);
	else if (texindex == 1)
		color = uTexture1.Sample(uSampler1, input.texcoords);
	else if (texindex == 2)
		color = uTexture2.Sample(uSampler2, input.texcoords);
	else if (texindex == 3)
		color = uTexture3.Sample(uSampler3, input.texcoords);
	else
		color = uTexture0.Sample(uSampler0, input.texcoords);
		
	if (texkind == 1)
		color.w = pow(abs(max(color.x, max(color.y, color.z))), 2.2);

	return color * input.color;
}
