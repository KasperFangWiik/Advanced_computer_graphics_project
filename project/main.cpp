
#ifdef _WIN32
extern "C" _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
#endif

#include <GL/glew.h>
#include <cmath>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>

#include <perf.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;

#include <Model.h>
#include "hdr.h"
#include "fbo.h"

#include "p_header/essential_collision.h"
using namespace col;
///////////////////////////////////////////////////////////////////////////////
// Various globals
///////////////////////////////////////////////////////////////////////////////
SDL_Window* g_window = nullptr;
float currentTime = 0.0f;
float previousTime = 0.0f;
float deltaTime = 0.0f;
int windowWidth, windowHeight;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

///////////////////////////////////////////////////////////////////////////////
// Shader programs
///////////////////////////////////////////////////////////////////////////////
GLuint shaderProgram;       // Shader for rendering the final image
GLuint simpleShaderProgram; // Shader used to draw the shadow map
GLuint backgroundProgram;

///////////////////////////////////////////////////////////////////////////////
// Environment
///////////////////////////////////////////////////////////////////////////////
float environment_multiplier = 1.5f;
GLuint environmentMap;
const std::string envmap_base_name = "001";

///////////////////////////////////////////////////////////////////////////////
// Light source
///////////////////////////////////////////////////////////////////////////////
vec3 lightPosition;
vec3 point_light_color = vec3(1.f, 1.f, 1.f);

float point_light_intensity_multiplier = 10000.0f;




///////////////////////////////////////////////////////////////////////////////
// Camera parameters.
///////////////////////////////////////////////////////////////////////////////
vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
float cameraSpeed = 50.f;//10.f;

vec3 worldUp(0.0f, 1.0f, 0.0f);

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

labhelper::Model* testsceneModel = nullptr;
labhelper::Model* entityCubeModel = nullptr;

std::vector<ConvexCollider> ColliderList;
ConvexCollider entityCollider;

mat4 entityCubeModelMatrix;
mat4 testsceneModelMatrix;

mat4 roomModelMatrix;
mat4 landingPadModelMatrix;
mat4 fighterModelMatrix;

void loadShaders(bool is_reload)
{
	GLuint shader = labhelper::loadShaderProgram("../project/simple.vert", "../project/simple.frag", is_reload);
	if(shader != 0)
	{
		simpleShaderProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/background.vert", "../project/background.frag", is_reload);
	if(shader != 0)
	{
		backgroundProgram = shader;
	}

	shader = labhelper::loadShaderProgram("../project/shading.vert", "../project/shading.frag", is_reload);
	if(shader != 0)
	{
		shaderProgram = shader;
	}
}


void print_vec3(std::vector<vec3>& vertices){
		for(const glm::vec3& v: vertices)
			printf("{ %f, %f , %f } ", v.x, v.y, v.z);
}

///////////////////////////////////////////////////////////////////////////////
/// This function is called once at the start of the program and never again
///////////////////////////////////////////////////////////////////////////////
void initialize()
{
	ENSURE_INITIALIZE_ONLY_ONCE();

	///////////////////////////////////////////////////////////////////////
	//		Load Shaders
	///////////////////////////////////////////////////////////////////////
	loadShaders(false);

	///////////////////////////////////////////////////////////////////////
	// Load models and set up model matrices
	///////////////////////////////////////////////////////////////////////
	fighterModel = labhelper::loadModelFromOBJ("../scenes/NewShip.obj");
	landingpadModel = labhelper::loadModelFromOBJ("../scenes/landingpad.obj");
	sphereModel = labhelper::loadModelFromOBJ("../scenes/sphere.obj");

	//testsceneModel = labhelper::loadModelFromOBJ("../scenes/test_scene1.obj");
	testsceneModel = labhelper::loadModelFromOBJ_n_addColiderFile("../scenes/test_scene1.obj");

	entityCubeModel = labhelper::loadModelFromOBJ("../scenes/test_ent.obj");;
	//entityCubeModel = labhelper::loadModelFromOBJ_n_addColiderFile("../scenes/test_ent.obj");;


	entityCollider = {labhelper::loadColliders("../scenes/test_ent_Cube.dat"), 0};

		// loadColliders returns a std::vector<glm::vec3> type
	printf("entityCollider: \n");
	for(glm::vec3& v: entityCollider.vertices){
		v += glm::vec3(0.0f,1.0f, 0.0f); // motsvarar translation
		printf("{ %f, %f , %f } ", v.x, v.y, v.z);
	}
	printf("\n");

	//std::vector<glm::vec3> collider{};
	//collider = labhelper::loadColliders("../scenes/test_scene1_East_Wall_East_Wall_Material.dat");

	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_East_Wall_East_Wall_Material.dat"), 1);
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_North_Wall_North_Wall_Material.dat"), 2);
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_South_Wall_South_Wall_Material.dat"), 3);
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_West_Wall_West_Wall_Material.dat"), 4);

	/*
	// this lead to problem where outside of initialize scoop the ColliderList ConvexCollider's vertices where empty why....?
	ConvexCollider c1 = {labhelper::loadColliders("../scenes/test_scene1_East_Wall_East_Wall_Material.dat"), 1};
	ConvexCollider c2 = {labhelper::loadColliders("../scenes/test_scene1_North_Wall_North_Wall_Material.dat"), 2};
	ConvexCollider c3 = {labhelper::loadColliders("../scenes/test_scene1_South_Wall_South_Wall_Material.dat"), 3};
	ConvexCollider c4 = {labhelper::loadColliders("../scenes/test_scene1_West_Wall_West_Wall_Material.dat"), 4};

	printf("entity: %zu \n",entityCollider.vertices.size());
	printf("c1: %zu \n",c1.vertices.size());
	printf("c2: %zu \n",c2.vertices.size());
	printf("c3: %zu \n",c3.vertices.size());
	printf("c4: %zu \n",c4.vertices.size());
	*/

	/*
	printf("inside ColliderList: \n");
	for(ConvexCollider& c: ColliderList){
		printf("c: %zu ",c.vertices.size());
		//std::vector<glm::vec3> vertecis_poly1(std::move(c.vertices));
		//std::vector<glm::vec3>  normals_1(std::move(normals_of_ConvexShape(vertecis_poly1)));
		//print_vec3(normals_1);
		printf("\n");

	}
	*/

	
	printf("ColliderList[0] normals: \n");
	std::vector<glm::vec3>  normals_1(normals_of_ConvexShape(ColliderList[0].vertices));
	print_vec3(normals_1);
	printf("\n");
	
	printf("Entity normals: \n");
	std::vector<glm::vec3>  normals_e(normals_of_ConvexShape(entityCollider.vertices));
	print_vec3(normals_e);
	printf("\n");
	

	/*
	std::cout << "c1: " << c1.vertices.size() << '\n';
	std::cout << "c2: " << c2.vertices.size() << '\n';
	std::cout << "c3: " << c3.vertices.size() << '\n';
	std::cout << "c4: " << c4.vertices.size() << '\n';
	*/
	// how and could i handle a multi file path like: "../scenes/test_scene1_*_Wall_*.dat"
	/*
	//find all file names in directory and find the ones containing these?
	if (filename.find("test_scene1_") != std::string::npos &&
    filename.find("_Wall_") != std::string::npos &&
    filename.ends_with(".dat"))
	*/

	
	//Get_2dEdgeVertices_of_convexShapeModel(*testsceneModel);

	testsceneModelMatrix = mat4(1.0f);
		// borde göra något mer regoröst..
	entityCubeModelMatrix = glm::translate(glm::vec3(0.0f,1.0f, 0.0f)) * mat4(1.0f);

	//entityCollider = glm::translate(glm::vec3(0.0f,1.5f, 0.0f)) * entityCollider; 


	// entityCubeModelMatrix = glm::translate(glm::vec3(0.0f,0.5f, 0.0f)) * entityCubeModelMatrix;
	
	roomModelMatrix = mat4(1.0f);
	fighterModelMatrix = translate(15.0f * worldUp);
	landingPadModelMatrix = mat4(1.0f);

	///////////////////////////////////////////////////////////////////////
	// Load environment map
	///////////////////////////////////////////////////////////////////////
	environmentMap = labhelper::loadHdrTexture("../scenes/envmaps/" + envmap_base_name + ".hdr");


	glEnable(GL_DEPTH_TEST); // enable Z-buffering
	glEnable(GL_CULL_FACE);  // enables backface culling
}

void debugDrawLight(const glm::mat4& viewMatrix,
                    const glm::mat4& projectionMatrix,
                    const glm::vec3& worldSpaceLightPos)
{
	mat4 modelMatrix = glm::translate(worldSpaceLightPos);
	glUseProgram(shaderProgram);
	labhelper::setUniformSlow(shaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	labhelper::render(sphereModel);
}


void drawBackground(const mat4& viewMatrix, const mat4& projectionMatrix)
{
	glUseProgram(backgroundProgram);
	labhelper::setUniformSlow(backgroundProgram, "environment_multiplier", environment_multiplier);
	labhelper::setUniformSlow(backgroundProgram, "inv_PV", inverse(projectionMatrix * viewMatrix));
	labhelper::setUniformSlow(backgroundProgram, "camera_pos", cameraPosition);
	labhelper::drawFullScreenQuad();
}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to draw the main objects on the scene
///////////////////////////////////////////////////////////////////////////////
void drawScene(GLuint currentShaderProgram,
               const mat4& viewMatrix,
               const mat4& projectionMatrix,
               const mat4& lightViewMatrix,
               const mat4& lightProjectionMatrix)
{
	glUseProgram(currentShaderProgram);
	// Light source
	vec4 viewSpaceLightPosition = viewMatrix * vec4(lightPosition, 1.0f);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_color", point_light_color);
	labhelper::setUniformSlow(currentShaderProgram, "point_light_intensity_multiplier",
	                          point_light_intensity_multiplier);
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightPosition", vec3(viewSpaceLightPosition));
	labhelper::setUniformSlow(currentShaderProgram, "viewSpaceLightDir",
	                          normalize(vec3(viewMatrix * vec4(-lightPosition, 0.0f))));


	// Environment
	labhelper::setUniformSlow(currentShaderProgram, "environment_multiplier", environment_multiplier);

	// camera
	labhelper::setUniformSlow(currentShaderProgram, "viewInverse", inverse(viewMatrix));

	/*
	// landing pad
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * landingPadModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * landingPadModelMatrix)));

	labhelper::render(landingpadModel);
	*/

	// test scene
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * testsceneModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * testsceneModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * testsceneModelMatrix)));

	labhelper::render(testsceneModel);
	
	//render entity cube
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * entityCubeModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * entityCubeModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * entityCubeModelMatrix)));

	labhelper::render(entityCubeModel);
	
	

	// Fighter
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * fighterModelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * fighterModelMatrix)));

	labhelper::render(fighterModel);
}


///////////////////////////////////////////////////////////////////////////////
/// This function will be called once per frame, so the code to set up
/// the scene for rendering should go here
///////////////////////////////////////////////////////////////////////////////
void display(void)
{
	labhelper::perf::Scope s( "Display" );

	///////////////////////////////////////////////////////////////////////////
	// Check if window size has changed and resize buffers as needed
	///////////////////////////////////////////////////////////////////////////
	{
		int w, h;
		SDL_GetWindowSize(g_window, &w, &h);
		if(w != windowWidth || h != windowHeight)
		{
			windowWidth = w;
			windowHeight = h;
		}
	}

	///////////////////////////////////////////////////////////////////////////
	// setup matrices
	///////////////////////////////////////////////////////////////////////////
	mat4 projMatrix = perspective(radians(45.0f), float(windowWidth) / float(windowHeight), 5.0f, 2000.0f);
	mat4 viewMatrix = lookAt(cameraPosition, cameraPosition + cameraDirection, worldUp);

	vec4 lightStartPosition = vec4(40.0f, 40.0f, 0.0f, 1.0f);
	lightPosition = vec3(rotate(currentTime, worldUp) * lightStartPosition);
	mat4 lightViewMatrix = lookAt(lightPosition, vec3(0.0f), worldUp);
	mat4 lightProjMatrix = perspective(radians(45.0f), 1.0f, 25.0f, 100.0f);

	///////////////////////////////////////////////////////////////////////////
	// Bind the environment map(s) to unused texture units
	///////////////////////////////////////////////////////////////////////////
	glActiveTexture(GL_TEXTURE6);
	glBindTexture(GL_TEXTURE_2D, environmentMap);
	glActiveTexture(GL_TEXTURE0);


	///////////////////////////////////////////////////////////////////////////
	// Draw from camera
	///////////////////////////////////////////////////////////////////////////
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, windowWidth, windowHeight);
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	{
		labhelper::perf::Scope s( "Background" );
		drawBackground(viewMatrix, projMatrix);
	}
	{
		labhelper::perf::Scope s( "Scene" );
		drawScene( shaderProgram, viewMatrix, projMatrix, lightViewMatrix, lightProjMatrix );
	}
	debugDrawLight(viewMatrix, projMatrix, vec3(lightPosition));

}


///////////////////////////////////////////////////////////////////////////////
/// This function is used to update the scene according to user input
///////////////////////////////////////////////////////////////////////////////
bool handleEvents(void)
{
	// check events (keyboard among other)
	SDL_Event event;
	bool quitEvent = false;
	while(SDL_PollEvent(&event))
	{
		labhelper::processEvent( &event );

		if(event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
		{
			quitEvent = true;
		}
		if(event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
		{
			if ( labhelper::isGUIvisible() )
			{
				labhelper::hideGUI();
			}
			else
			{
				labhelper::showGUI();
			}
		}
		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
		   && (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			g_prevMouseCoords.x = x;
			g_prevMouseCoords.y = y;
		}

		if(!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
		{
			g_isMouseDragging = false;
		}

		if(event.type == SDL_MOUSEMOTION && g_isMouseDragging)
		{
			// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
			int delta_x = event.motion.x - g_prevMouseCoords.x;
			int delta_y = event.motion.y - g_prevMouseCoords.y;
			float rotationSpeed = 0.1f;
			mat4 yaw = rotate(rotationSpeed * deltaTime * -delta_x, worldUp);
			mat4 pitch = rotate(rotationSpeed * deltaTime * -delta_y,
			                    normalize(cross(cameraDirection, worldUp)));
			cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			g_prevMouseCoords.x = event.motion.x;
			g_prevMouseCoords.y = event.motion.y;
		}
	}

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);
	vec3 cameraRight = cross(cameraDirection, worldUp);

	if(state[SDL_SCANCODE_W])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_S])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraDirection;
	}
	if(state[SDL_SCANCODE_A])
	{
		cameraPosition -= cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_D])
	{
		cameraPosition += cameraSpeed * deltaTime * cameraRight;
	}
	if(state[SDL_SCANCODE_Q])
	{
		cameraPosition -= cameraSpeed * deltaTime * worldUp;
	}
	if(state[SDL_SCANCODE_E])
	{
		cameraPosition += cameraSpeed * deltaTime * worldUp;
	}

	// entity movement: move along the test_scene plane, for now x and z axis
	if(state[SDL_SCANCODE_UP] || state[SDL_SCANCODE_DOWN] ||
	   state[SDL_SCANCODE_LEFT] || state[SDL_SCANCODE_RIGHT]){
		/*
		bool up = state[SDL_SCANCODE_UP];
		bool down = state[SDL_SCANCODE_DOWN];

		bool left = state[SDL_SCANCODE_LEFT];
		bool right = state[SDL_SCANCODE_RIGHT];
		*/

		bool up = state[SDL_SCANCODE_RIGHT] ;
		bool down = state[SDL_SCANCODE_LEFT];

		bool left = state[SDL_SCANCODE_DOWN];
		bool right = state[SDL_SCANCODE_UP];

		 // != works like xor
		bool moveParallel = (up != down);
		bool moveVertical = (left != right);
		glm::vec3 mv = glm::vec3((float)moveVertical, 0.0f, -(float)moveParallel);

		// moving diagonaly
		const float ONE_DIV_SQRTWO = 0.70706781f;
		if ((up && right) || (up && left) || 
		  (down && right) || (down && left))
		{
			mv.x = ONE_DIV_SQRTWO; 
			mv.z = -ONE_DIV_SQRTWO;
		}

		if(up){ mv.z = -mv.z; }
		if(left){ mv.x = -mv.x; }
		
		float entitySpeed = 25.0f;
		entityCubeModelMatrix = glm::translate( mv * entitySpeed * deltaTime) * entityCubeModelMatrix;
		for(glm::vec3& v: entityCollider.vertices){
			glm::vec3 new_v = glm::vec3(glm::translate( mv * entitySpeed * deltaTime) * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
		}
			
		/*
		printf("what new vert: \n");
		print_vec3(entityCollider.vertices);
		printf("\n");
		*/


	   }


	return quitEvent;
}


///////////////////////////////////////////////////////////////////////////////
/// This function is to hold the general GUI logic
///////////////////////////////////////////////////////////////////////////////
void gui()
{
	// ----------------- Set variables --------------------------
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
	            ImGui::GetIO().Framerate);
	// ----------------------------------------------------------


	////////////////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////////////////

	labhelper::perf::drawEventsWindow();
}

void collision_resolution_test(){

	//std::vector<ConvexCollider> ColliderList;
	//std::vector<glm::vec3> entityCollider;
	glm::vec3 responsVector = glm::vec3(0.0f);	
	for(ConvexCollider& c: ColliderList){
		//printf("c: %zu \n",c.vertices.size());
		
		if(col::collision(entityCollider, c, responsVector)){
			entityCubeModelMatrix = glm::translate(responsVector) * entityCubeModelMatrix; // should glm::translate(responsVector*deltaTime)?

			for(glm::vec3& v: entityCollider.vertices){
			glm::vec3 new_v = glm::vec3(glm::translate(responsVector) * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
		}

			//printf(" collision :) \n");
		}
		
	}
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Project");

	initialize();

	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while(!stopRendering)
	{
		//update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		previousTime = currentTime;
		currentTime = timeSinceStart.count();
		deltaTime = currentTime - previousTime;

		// check events (keyboard among other)
		stopRendering = handleEvents();

		// collision check and resolution should be here
		//printf("time to print first collider: %zu ",ColliderList[0].vertices.size()  );
		//print_vec3(ColliderList[0].vertices);
		collision_resolution_test();

		// Inform imgui of new frame
		labhelper::newFrame( g_window );

		// render to window
		display();

		// Render overlay GUI.
		gui();

		// Finish the frame and render the GUI
		labhelper::finishFrame();

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);
	}
	// Free Models
	labhelper::freeModel(fighterModel);
	labhelper::freeModel(landingpadModel);
	labhelper::freeModel(sphereModel);

	labhelper::freeModel(testsceneModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
