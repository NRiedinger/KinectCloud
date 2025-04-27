#include "Application.h"
#include "ResourceManager.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <stdio.h>
#include <malloc.h>
#include <string.h>


int main(int, char**) {

	Application app;
	if (!app.on_init())
		return 1;

	while (app.is_running()) {
		app.on_frame();
	}

	app.on_finish();

	return 0;
}

