struct VS_INPUT
{
	float2 pos : VINPUT0;
	int2   texindex : VINPUT1;
	float2 texcoords : VINPUT2;
	float4 color : VINPUT3;
	
	uint vertexIndex : SV_VertexID;
};

struct VS_OUTPUT
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float2 texindex : TEXCOORD1;
	float2 rawpos : TEXCOORD2;
	float4 color : TEXCOORD3;
};

cbuffer UniformBuffer0 : register(b0)
{
	matrix uView;
};

VS_OUTPUT Vertex(VS_INPUT input)
{
	VS_OUTPUT output;
	
	uint index = input.vertexIndex;
	float2 rawpos = float2(index & 1, (index >> 1) & 1);
	float2 pos = input.pos;
	
	output.position = mul(uView, float4(pos, 0.0, 1.0));
	output.color = input.color;
	output.texcoords = input.texcoords;
	output.texindex = float2(input.texindex);
	output.rawpos = rawpos;
	
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
	float4 result = 0;
	float2 texsize;
	float2 uv = input.texcoords.xy;
	int uindex = abs(int(input.texindex.x));
	int uswizzle = abs(int(input.texindex.y));
	float2 centeredpos = (input.rawpos - 0.5) * 2.0;
	float2 uvfwidth = fwidth(uv);
	float2 scale = 1.0 / abs(fwidth(input.rawpos));
	
	switch (uindex)
	{
		default:result = uTexture0.Sample(uSampler0, uv); uTexture0.GetDimensions(texsize); break;
		case 1: result = uTexture1.Sample(uSampler1, uv); uTexture1.GetDimensions(texsize); break;
		case 2: result = uTexture2.Sample(uSampler2, uv); uTexture2.GetDimensions(texsize); break;
		case 3: result = uTexture3.Sample(uSampler3, uv); uTexture3.GetDimensions(texsize); break;
	}
	
	switch (uswizzle)
	{
		default: break;
		case 1:
		{
			// Derived from: https://www.shadertoy.com/view/4llXD7
			float r = 0.5 * max(scale.x, scale.y);
			float2 q = abs(centeredpos * scale) - scale + r;
			float dist = min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
			
			if (dist >= -1.0)
				result.xyzw *= max(1.0-(dist+1.0)*0.5, 0.0);
		} break;
		case 2:
		{
			float2 density = uvfwidth * texsize;
			float m = min(density.x, density.y);
			float inv = 1.0 / m;
			float a = (result.x - 128.0/255.0 + 24.0/255.0*m*0.5) * (255.0/24.0) * inv;
			result = a.xxxx;
		} break;
	}
	
	return result * input.color;
}
