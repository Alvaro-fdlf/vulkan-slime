#version 460 core

layout (location = 0) out vec4 colorOut;
layout (input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput colorIn;
layout (set = 0, binding = 0, rgba8) uniform readonly image2D frontImg;

layout (constant_id = 0) const float redFade = 0;
layout (constant_id = 1) const float greenFade = 0;
layout (constant_id = 2) const float blueFade = 0;
layout (constant_id = 3) const float blur1 = 1;
layout (constant_id = 4) const float blur2 = 1;
layout (constant_id = 5) const float blur3 = 1;
layout (constant_id = 6) const float blur4 = 1;
layout (constant_id = 7) const float blur5 = 1;
layout (constant_id = 8) const float blur6 = 1;
layout (constant_id = 9) const float blur7 = 1;
layout (constant_id = 10) const float blur8 = 1;
layout (constant_id = 11) const float blur9 = 1;
layout (constant_id = 12) const float blurDivide = 9;
layout (constant_id = 13) const float particleR = 1;
layout (constant_id = 14) const float particleG = 1;
layout (constant_id = 15) const float particleB = 1;

void main(void) {
	// format may be different from expected for some reason
	vec4 curColor = subpassLoad(colorIn);
	if (curColor.a > 0.4 && curColor.a < 0.6) {
		colorOut = vec4(particleB, particleG, particleR, 1.0);
	}
	else {
		// Blur
		vec4 ul = imageLoad(frontImg, ivec2(gl_FragCoord.x-1, gl_FragCoord.y-1));
		vec4 uc = imageLoad(frontImg, ivec2(gl_FragCoord.x,   gl_FragCoord.y-1));
		vec4 ur = imageLoad(frontImg, ivec2(gl_FragCoord.x+1, gl_FragCoord.y-1));
		vec4 cl = imageLoad(frontImg, ivec2(gl_FragCoord.x-1, gl_FragCoord.y  ));
		vec4 cc = imageLoad(frontImg, ivec2(gl_FragCoord.x,   gl_FragCoord.y  ));
		vec4 cr = imageLoad(frontImg, ivec2(gl_FragCoord.x+1, gl_FragCoord.y  ));
		vec4 dl = imageLoad(frontImg, ivec2(gl_FragCoord.x-1, gl_FragCoord.y+1));
		vec4 dc = imageLoad(frontImg, ivec2(gl_FragCoord.x,   gl_FragCoord.y+1));
		vec4 dr = imageLoad(frontImg, ivec2(gl_FragCoord.x+1, gl_FragCoord.y+1));
		vec4 outPixel = ul*blur1 + uc*blur2 + ur*blur3 +
				cl*blur4 + cc*blur5 + cr*blur6 +
				dl*blur7 + dc*blur8 + dr*blur9;

		outPixel /= blurDivide;

		// Fade
		outPixel.r -= redFade;
		outPixel.g -= greenFade;
		outPixel.b -= blueFade;

		outPixel.a = 1.0;
		colorOut = outPixel;
	}
}
