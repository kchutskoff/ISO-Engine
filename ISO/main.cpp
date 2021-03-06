#include <SFML/Graphics.hpp>
#include <chrono>
#include <thread>
#include <sstream>
#include <iomanip>
#include <string>
#include "map.h"
#include "tileset.h"
#include <stdlib.h>
#include <Windows.h>
#include "JobPool.h"
#include "texture.h"
#include "FileHandle.h"

class job_preDrawMap : public ISO::JobPool::Job
{
public:
	ISO::Map* map;
	sf::Vector2f* cameraPos;
	sf::Vector2u* winSize;

	job_preDrawMap(	ISO::Map* whichMap,
					sf::Vector2f* cameraPosition,
					sf::Vector2u* windowSize) 
					: map(whichMap), cameraPos(cameraPosition), winSize(windowSize) {}

private:
	void operator()()
	{
		map->preDraw(*cameraPos, *winSize);
	}
};

class job_getInterpolationCamera : public ISO::JobPool::Job
{
public:
	sf::Vector2f* oldCam;
	sf::Vector2f* newCam;
	sf::Vector2f* intCam;
	float* i;

	job_getInterpolationCamera( sf::Vector2f* newCamera,
								sf::Vector2f* oldCamera,
								float* interpolation,
								sf::Vector2f* interpolatedCamera) 
								: oldCam(oldCamera), newCam(newCamera), i(interpolation), intCam(interpolatedCamera) {}

private:
	void operator()()
	{
		intCam->x = oldCam->x * (1 - *i) + newCam->x * (*i);
		intCam->y = oldCam->y * (1 - *i) + newCam->y * (*i);
	}
};

class job_updateCamera : public ISO::JobPool::Job
{
public:
	std::vector<bool>* keys;
	sf::Vector2f* cameraPos;
	sf::Vector2f* oldCamPos;

	job_updateCamera( std::vector<bool>* keysPressed,
					sf::Vector2f* cameraPosition,
					sf::Vector2f* oldCameraPosition ) 
					: keys(keysPressed), cameraPos(cameraPosition), oldCamPos(oldCameraPosition) {}

private:
	void operator()()
	{
		*oldCamPos = *cameraPos;
		if((*keys)[sf::Keyboard::Left])
			cameraPos->x -= 5;
		if((*keys)[sf::Keyboard::Right])
			cameraPos->x += 5;
		if((*keys)[sf::Keyboard::Up])
			cameraPos->y -= 5;
		if((*keys)[sf::Keyboard::Down])
			cameraPos->y += 5;
	}
};

class job_updateViews : public ISO::JobPool::Job
{
public:
	sf::Vector2f* cameraPos;
	sf::Vector2u* winSize;
	sf::View* worldView;
	sf::View* uiView;

	job_updateViews(	sf::Vector2f* cameraPosition,
						sf::Vector2u* windowSize,
						sf::View* worldView,
						sf::View* uiView)
						: cameraPos(cameraPosition), winSize(windowSize), worldView(worldView), uiView(uiView) {}

private:
	void operator()()
	{
		worldView->setSize(static_cast<float>(winSize->x), static_cast<float>(winSize->y));
		uiView->setSize(static_cast<float>(winSize->x), static_cast<float>(winSize->y));

		worldView->setCenter(*cameraPos);
		uiView->setCenter(static_cast<float>(winSize->x) / 2.f, static_cast<float>(winSize->y) / 2.f);
	}
};

const sf::Uint64 MICROSECONDS_PER_SECOND = 1000000;

int main()
{
	std::vector<bool> keyState(256, false);
    sf::RenderWindow window(sf::VideoMode(800, 600), "ISO Engine");

	float interpolate = 0;

	ISO::FileHandle::addDirectory("assets/", 0, false);
	ISO::FileHandle::addDirectory("output/", 1, true);

	ISO::Texture::init();

	sf::Vector2u windowSize(800,600);
	sf::Vector2f cameraPos(0,0);
	sf::Vector2f oldCameraPos(0,0);
	sf::Vector2f interpolatedCamera(0,0);
	sf::View worldView = window.getView();
	sf::View uiView = window.getView();

	sf::Clock gameClock;

	sf::Font consola;
	consola.loadFromFile("assets/fonts/consola.ttf");
	sf::Text infoText("", consola, 15);
	infoText.setColor(sf::Color::White);

	ISO::tileset grass("grass_new");
	ISO::Map mymap(5,5,2,&grass);

	mymap.getMapTile(0,0,0)->setHeight(6);
	mymap.getMapTile(0,0,0)->setBaseTill(5);
	mymap.addTileToMap(0,0,2);

	mymap.getMapTile(0,1,0)->setHeight(6);
	mymap.getMapTile(0,1,0)->setBaseTill(5);
	mymap.addTileToMap(0,1,2);

	mymap.getMapTile(1,0,0)->setHeight(5);
	mymap.getMapTile(1,0,0)->setType(true, true, false, false);

	mymap.getMapTile(2,0,0)->setHeight(4);
	mymap.getMapTile(2,0,0)->setType(true, true, false, false);

	mymap.getMapTile(3,0,0)->setHeight(4);
	
	mymap.getMapTile(3,1,0)->setHeight(3);
	mymap.getMapTile(3,1,0)->setType(true, false, false, true);

	mymap.getMapTile(3,2,0)->setHeight(3);

	mymap.getMapTile(4,1,0)->setType(false, true, false, false);

	mymap.getMapTile(4,2,0)->setType(true, true, false, false);

	mymap.getMapTile(4,3,0)->setType(true, false, false, false);

	mymap.getMapTile(3,3,0)->setType(true, false, false, true);

	mymap.getMapTile(2,3,0)->setType(false, false, false, true);

	mymap.getMapTile(2,2,0)->setType(true, false, true, true);

	mymap.getMapTile(2,1,0)->setHeight(3);
	mymap.getMapTile(2,1,0)->setType(false, false, false, true);

	mymap.getMapTile(1,1,0)->setType(false, false, true, true);

	mymap.getMapTile(1,2,0)->setType(false, false, false, true);

	mymap.addTileToMap(1,1,6,3,NULL,true, 5);
	mymap.addTileToMap(2,1,7,3,NULL,true, 6);
	mymap.addTileToMap(3,1,8,0,NULL,true, 7);

	mymap.getMapTile(4,4,0)->setDrawBase(false);

	//mymap.setSize(10, 8);

	mymap.saveToFile("testmap.xml");

	mymap.loadFromFile("testmap.xml");

	sf::Uint64 targetFPS = 60;
	sf::Uint64 targetMicrosecond = MICROSECONDS_PER_SECOND / targetFPS;

	sf::Uint64 targetTPS = 30;
	sf::Uint64 targetGameTick = MICROSECONDS_PER_SECOND / targetTPS;

	sf::Uint64 gameTime = 0;
	sf::Uint64 currentTime = gameClock.getElapsedTime().asMicroseconds();
	sf::Uint64 accumulator = 0;

	// setup jobs

	ISO::JobPool workPool;

	job_updateCamera camJob(&keyState, &cameraPos, &oldCameraPos);

	job_getInterpolationCamera intCamJob(&cameraPos, &oldCameraPos, &interpolate, &interpolatedCamera );

	job_preDrawMap mapJob(&mymap, &interpolatedCamera, &windowSize);
	mapJob.addDependancy(&intCamJob);

	job_updateViews viewJob(&interpolatedCamera, &windowSize, &worldView, &uiView);
	viewJob.addDependancy(&intCamJob);

	const sf::Uint64 maxFrameTime = MICROSECONDS_PER_SECOND;
	        
	// main game loop
    while (window.isOpen())
    {
		// start next loop

		sf::Uint64 newTime = gameClock.getElapsedTime().asMicroseconds();
		sf::Uint64 frameTime = newTime - currentTime;
		sf::Uint64 actualFrameTime = frameTime;

		if(frameTime < targetMicrosecond)
		{
			unsigned long long delay = targetMicrosecond - frameTime;
			const unsigned long long error = 2000;
			if(delay > error * 2)
			{
				std::this_thread::sleep_for(std::chrono::microseconds(delay-error));
			}
			do
			{
				newTime = gameClock.getElapsedTime().asMicroseconds();
				frameTime = newTime - currentTime;
			}while(frameTime < targetMicrosecond);
			
		}

		if(frameTime > maxFrameTime)
		{
			frameTime = maxFrameTime;
		}
		currentTime = newTime;

		accumulator += frameTime;

		// handle events before next loop
        sf::Event event;
        while (window.pollEvent(event))
        {
            if (event.type == sf::Event::Closed)
            {
                window.close();
			}else if(event.type == sf::Event::Resized)
			{
				windowSize.x = event.size.width;
				windowSize.y = event.size.height;
            }else if(event.type == sf::Event::KeyPressed)
			{
				if(event.key.code < 256 && event.key.code >= 0)
				{
					keyState[event.key.code] = true;
				}
			}else if(event.type = sf::Event::KeyReleased)
			{
				if(event.key.code < 256 && event.key.code >= 0)
				{
					keyState[event.key.code] = false;
				}
			}
        }

		while( accumulator >= targetGameTick)
		{
			// update game	

			workPool.addJobToPool(&camJob);
			
			workPool.waitForJobs();

			// end work
			gameTime += targetGameTick;
			accumulator -= targetGameTick;
		}

		interpolate = static_cast<float>(accumulator) / static_cast<float>(targetGameTick);

		// render with interpolation

			workPool.addJobToPool(&intCamJob);
			workPool.addJobToPool(&mapJob);
			workPool.addJobToPool(&viewJob);

			workPool.waitForJobs();
		
		std::stringstream infoBuffer;

		double FPS = static_cast<double>(MICROSECONDS_PER_SECOND) / static_cast<double>(frameTime);
		double actualLoad = static_cast<double>(actualFrameTime) / static_cast<double>(targetMicrosecond) *100;
		
		infoBuffer << "FPS: " << std::fixed << std::setprecision(2) << std::setw(6) << FPS << "  Load: " << std::setw(6) << actualLoad;
		infoBuffer << "\nCAM: " << std::setw(9) << interpolatedCamera.x << ", " << std::setw(9) << interpolatedCamera.y;
		infoBuffer << "\nACC: " << std::setw(5) << accumulator << " " << interpolate << "  FRM: " << std::setw(5) << frameTime;

		infoText.setString(infoBuffer.str());

        window.clear();
		// draw world
		window.setView(worldView);
		window.draw(mymap);

		// draw UI
		window.setView(uiView);

		window.draw(infoText);
        window.display(); 
    }
	ISO::Texture::uninit();

    return 0;
}