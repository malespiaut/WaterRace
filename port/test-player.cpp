// Logo Intro load test (issue #43): opens a GL window (xvfb-run), then runs InfinityPlayer_LoadAnimation on
// "WaterRace Data/Core/Logo Intro" with the default texture set, as Demo_Play does. Logs the script's animation
// count, duration and the uploaded texture sizes, and checks they are plausible.
// Linked with every game file; Boot.cp is included here so its main() can be renamed.
// Usage: test-player [GAME_ROOT]   (GAME_ROOT holds "WaterRace Data/")

#include <cmath>
#include <unistd.h>

#define main WaterRace_Main
#include "Boot.cp"
#undef main

#include "Infinity Player.h"

static int		gFailures = 0;

#define CHECK(cond) do { if(!(cond)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++gFailures; } } while(0)

struct TQATexture { GLuint name; bool mipmap; };	// mirrors OGL_Support.cp

int main(int argc, char** argv)
{
	const char*		root = argc > 1 ? argv[1] : WATERRACE_ROOT;
	FSSpec			spec;
	MegaObjectPtr	mega;
	OSErr			err;
	long			textured = 0;

	if(chdir(root) != 0) {
		perror(root);
		return 1;
	}
	Pomme::Init();
	if(!Boot_InitGL() || InfinityPlayer_Init(false) != noErr) {
		fprintf(stderr, "FAIL: GL window or InfinityPlayer_Init\n");
		return 1;
	}
	if(FSMakeFSSpec(0, 0, ":WaterRace Data:Core:Logo Intro", &spec) != noErr) {
		fprintf(stderr, "FAIL: Logo Intro not found\n");
		return 1;
	}

	err = InfinityPlayer_LoadAnimation(nil, nil, &spec, kPlayer_DefaultTextureSet, 0, 0, kWindowWidth, kWindowHeight, 0, 0, nil, 0, 0, false);
	printf("InfinityPlayer_LoadAnimation: %d\n", err);
	CHECK(err == noErr);
	mega = InfinityPlayer_ReturnMegaObject();
	if(err != noErr || mega == nil)
	return 1;

	// Model: 16 shapes, one script, cameras skipped by the player
	printf("mega %.*s: %lu shapes, %ld scripts, %ld boxes\n", mega->object.name[0], (char*) mega->object.name + 1, mega->object.shapeCount, mega->scriptCount, mega->boxCount);
	CHECK(mega->object.shapeCount == 16 && mega->scriptCount == 1 && mega->cameraCount == 0);
	for(long s = 0; s < mega->scriptCount; ++s) {
		ScriptPtr		script = mega->scriptList[s];

		printf("script %.*s: %lu animations, length %ld (%.2f s), loop %ld\n", script->name[0], (char*) script->name + 1, script->animationCount, script->length, (float) script->length / kTimeUnit, script->loopTime);
		CHECK(script->animationCount == 10 && script->length == 850);
		for(unsigned long a = 0; a < script->animationCount; ++a) {
			AnimationPtr	anim = script->animationList[a];
			ShapePtr		shape = (ShapePtr) anim->shapeID;	// resolved by Script_Resolve: must survive as a full 64-bit pointer
			bool			found = false;

			for(unsigned long i = 0; i < mega->object.shapeCount; ++i)
			found |= mega->object.shapeList[i] == shape;
			CHECK(found);
			if(!found)
			continue;
			printf("  anim %c%c%c%c (%.*s): %lu events, end %ld\n", (char) (shape->id >> 24), (char) (shape->id >> 16), (char) (shape->id >> 8), (char) shape->id,
				shape->name[0], (char*) shape->name + 1, anim->eventCount, anim->endTime);
			CHECK(anim->type == kType_Animation && anim->eventCount > 0 && anim->endTime > 0 && anim->endTime <= script->length);
			for(unsigned long e = 0; e < anim->eventCount; ++e) {
				const Event&	ev = anim->eventList[e];

				CHECK(ev.time >= 0 && ev.time <= anim->endTime && (e == 0 || ev.time > anim->eventList[e - 1].time));
				CHECK(std::isfinite(ev.rotateX) && std::isfinite(ev.rotateY) && std::isfinite(ev.rotateZ) && std::isfinite(ev.rotateW));
				CHECK(std::isfinite(ev.position.x) && std::isfinite(ev.position.y) && std::isfinite(ev.position.z) && std::fabs(ev.position.x) < 1000);
			}
			CHECK(anim->eventList[anim->eventCount - 1].time == anim->endTime);
		}
	}

	// Textures: every textured shape has a GL texture of a plausible size
	for(unsigned long i = 0; i < mega->object.shapeCount; ++i) {
		ShapePtr		shape = mega->object.shapeList[i];
		GLint			width = 0,
						height = 0;

		if(shape->texture == kNoTexture)
		continue;
		CHECK(shape->texturePtr != nil);
		if(shape->texturePtr == nil)
		continue;
		glBindTexture(GL_TEXTURE_2D, shape->texturePtr->name);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
		glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
		printf("  shape %.*s texture %c%c%c%c: %dx%d\n", shape->name[0], (char*) shape->name + 1,
			(char) (shape->texture >> 24), (char) (shape->texture >> 16), (char) (shape->texture >> 8), (char) shape->texture, width, height);
		CHECK(width >= 8 && width <= kMaxTextureSizeH && height >= 8 && height <= kMaxTextureSizeH);
		++textured;
	}
	CHECK(textured > 0);

	InfinityPlayer_UnloadAnimation();
	InfinityPlayer_Quit();
	if(gFailures)
	fprintf(stderr, "%d failure(s)\n", gFailures);
	else
	printf("OK\n");
	return gFailures ? 1 : 0;
}
