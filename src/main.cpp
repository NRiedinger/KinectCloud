#include "Application.h"
#include "ResourceManager.h"

int main(int, char**) {

	/*
	std::unordered_map<int64_t, Point3D> points;
	ResourceManager::readPoints3D(RESOURCE_DIR "/points3D.bin", points);
	*/
	
	Application app;

	if (!app.onInit()) {
		return 1;
	}

	while (app.isRunning()) {
		app.onFrame();
	}

	app.onFinish();

	return 0;
}
