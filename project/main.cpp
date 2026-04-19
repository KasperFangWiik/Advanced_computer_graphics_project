
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
// Entity
///////////////////////////////////////////////////////////////////////////////

class Entity {
	//private:
	public:
	std::vector<glm::vec3> collider; // probably dont need id's....
	glm::mat4 modelMatrix;
	glm::vec3 dir;
	bool active;

	//public:
	void move_entity(glm::mat4 matrix){
		modelMatrix = matrix * modelMatrix;
		for(glm::vec3& v: collider){
			glm::vec3 new_v = glm::vec3(matrix * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
		}
	}

	void move_entity_circle(glm::mat4 matrix){
		modelMatrix = matrix * modelMatrix;
		glm::vec3 new_v = glm::vec3(matrix * glm::vec4(collider[0] , 1.0f));
		collider[0] = glm::vec3(new_v.x,0.0f,new_v.z);
	}
};

///////////////////////////////////////////////////////////////////////////////
// Models
///////////////////////////////////////////////////////////////////////////////
labhelper::Model* fighterModel = nullptr;
labhelper::Model* landingpadModel = nullptr;
labhelper::Model* sphereModel = nullptr;

labhelper::Model* testsceneModel = nullptr;
labhelper::Model* entityCubeModel = nullptr;
labhelper::Model* testSphereModel = nullptr;

std::vector<std::vector<glm::vec3>> ColliderList;
std::vector<glm::vec3> entityCollider;
std::vector<glm::vec3> testSphereCollider;

std::vector<glm::vec3> enemyCubeCollider;

mat4 testSphereModelMatrix;

mat4 testSphereModelMatrix2;

std::vector<glm::mat4> ListSphereModelMatrix;
std::vector<Entity> ProjectileList; //should i have this
bool projectile_exists;
unsigned int bounce_counter;

mat4 entityCubeModelMatrix;
vec3 oldDir;

mat4 enemyCubeModelMatrix;
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

	testsceneModel = labhelper::loadModelFromOBJ("../scenes/test_scene1.obj");
	//testsceneModel = labhelper::loadModelFromOBJ_n_addColiderFile("../scenes/test_scene1.obj");

	entityCubeModel = labhelper::loadModelFromOBJ("../scenes/test_ent.obj");
	//entityCubeModel = labhelper::loadModelFromOBJ_n_addColiderFile("../scenes/test_ent.obj");

	testSphereModel = labhelper::loadModelFromOBJ("../scenes/Mball.obj");


	oldDir = vec3(0.0f);
	testSphereCollider = {glm::vec3(0.0f), glm::vec3(0.557555f,0.0f,0.0f)};

	ListSphereModelMatrix.reserve(5);
	ProjectileList.reserve(5);
	projectile_exists = false;
	bounce_counter = 0;
	ProjectileList.emplace_back(Entity{testSphereCollider,glm::mat4(0.0f),glm::vec3(0.0f),false});


	entityCollider = labhelper::loadColliders("../scenes/test_ent_Cube.dat");
	enemyCubeCollider = labhelper::loadColliders("../scenes/test_ent_Cube.dat");


	// collider for stationary sphere
	testSphereCollider[0] += glm::vec3(10.0f, 0.0f, 0.0f); // glm::vec3(10.0f, 0.557555f, 0.0f);

		// loadColliders returns a std::vector<glm::vec3> type
	//printf("entityCollider: \n");
	for(glm::vec3& v: entityCollider){
		v += glm::vec3(0.0f,1.0f, 0.0f); // motsvarar translation
		//printf("{ %f, %f , %f } ", v.x, v.y, v.z);
	}
	//printf("\n");

	for(glm::vec3& v: enemyCubeCollider){
		//glm::vec3(0.0f,1.0f, 10.0f)
		v += glm::vec3(0.0f,1.0f, 10.0f); // motsvarar translation
	}

	//std::vector<glm::vec3> collider{};
	//collider = labhelper::loadColliders("../scenes/test_scene1_East_Wall_East_Wall_Material.dat");

	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_East_Wall_East_Wall_Material.dat"));
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_North_Wall_North_Wall_Material.dat"));
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_South_Wall_South_Wall_Material.dat"));
	ColliderList.emplace_back(labhelper::loadColliders("../scenes/test_scene1_West_Wall_West_Wall_Material.dat"));
	ColliderList.emplace_back(testSphereCollider);
	ColliderList.emplace_back(enemyCubeCollider);

	
	//printf("ColliderList[0] normals: \n");
	std::vector<glm::vec3>  normals_1(normals_of_ConvexShape(ColliderList[0]));
	//print_vec3(normals_1);
	//printf("\n");
	
	//printf("Entity normals: \n");
	std::vector<glm::vec3>  normals_e(normals_of_ConvexShape(entityCollider));
	//print_vec3(normals_e);
	//printf("\n");
	

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

	enemyCubeModelMatrix = glm::translate(glm::vec3(0.0f,1.0f, 10.0f)) * mat4(1.0f);

	testSphereModelMatrix = glm::translate(glm::vec3(10.0f,0.557555f, 0.0f)) * mat4(1.0f);
	
	testSphereModelMatrix2 = glm::translate(glm::vec3(-10.0f,0.557555f, 0.0f)) * mat4(1.0f);
	//testSphereModelMatrix = glm::translate(glm::vec3(10.0f,0.557555f, 0.0f)) * mat4(1.0f);

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


void UseSlowModelRender(
	GLuint currentShaderProgram, 
	const mat4& viewMatrix, 
	const mat4& projectionMatrix,
	const labhelper::Model* model, const glm::mat4& modelMatrix
){
	labhelper::setUniformSlow(currentShaderProgram, "modelViewProjectionMatrix",
	                          projectionMatrix * viewMatrix * modelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "modelViewMatrix", viewMatrix * modelMatrix);
	labhelper::setUniformSlow(currentShaderProgram, "normalMatrix",
	                          inverse(transpose(viewMatrix * modelMatrix)));
	
	labhelper::render(model);
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

	// landing pad
	//UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, landingpadModel, landingPadModelMatrix);

	// test scene
	UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, testsceneModel, testsceneModelMatrix);
	
	//render entity cube
	UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, entityCubeModel, entityCubeModelMatrix);

	// render enemy cube
	UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, entityCubeModel, enemyCubeModelMatrix);

	//render entity sphere 
	UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, testSphereModel, testSphereModelMatrix);

	//render entity sphere2 
	UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, testSphereModel, testSphereModelMatrix2);

	/*
	if(ListSphereModelMatrix.size()){
		for(const mat4& ModelMatrix: ListSphereModelMatrix){
			UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, testSphereModel, ModelMatrix);
		}
	}
		*/

	
	//if(ProjectileList.size()){
		for(const Entity& p: ProjectileList){
			if(p.active == true)
				UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix, testSphereModel, p.modelMatrix);
		}
	//}
	
	// Fighter
	//UseSlowModelRender(currentShaderProgram,viewMatrix,projectionMatrix,fighterModel, fighterModelMatrix);

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


		if(event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_RIGHT
		   && (!labhelper::isGUIvisible() || !ImGui::GetIO().WantCaptureMouse))
		{
			//g_isMouseDragging = true;
			int x;
			int y;
			SDL_GetMouseState(&x, &y);
			/*
			float wx = x / float(windowWidth);
			float wy = y / float(windowHeight);
			glm::unProject();

			*/

			// cameraPosition
			/*
			vec3 cameraPosition(-70.0f, 50.0f, 70.0f);
			vec3 cameraDirection = normalize(vec3(0.0f) - cameraPosition);
			
			
			projectionMatrix
         	worldSpaceLightPos
			*/

			// t = (–d –n•o) / (n•d) 
			/*
			vec3 o = cameraPosition; 
			vec3 d = normalize(vec3(x, y, 1.0f) - o); // srhould z be zero o
			vec3 n = vec3(0.0f, 1.0f, 0.0f);

			vec3 t = (-d * glm::dot(-n,o)) / glm::dot(n,d);
			vec3 pos = o + d*t;

			testSphereModelMatrix2 = glm::mat4(1.0f);
			//testSphereModelMatrix2 = glm::translate(glm::vec3(pos.x,0.557555f, pos.z)) * testSphereModelMatrix2;

			testSphereModelMatrix2 = glm::translate(pos) * testSphereModelMatrix2;
			*/

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
		if((up && right) || (up && left) || 
		   (down && right) || (down && left))
		{
			mv.x = ONE_DIV_SQRTWO; 
			mv.z = -ONE_DIV_SQRTWO;
		} 
		
		if(up){ mv.z = -mv.z; }
		if(left){ mv.x = -mv.x; }

				oldDir = mv;
		
		float entitySpeed = 15.0f;
		entityCubeModelMatrix = glm::translate( mv * entitySpeed * deltaTime) * entityCubeModelMatrix;

		for(glm::vec3& v: entityCollider){
			glm::vec3 new_v = glm::vec3(glm::translate( mv * entitySpeed * deltaTime) * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
		}
	   }

	   if(state[SDL_SCANCODE_SPACE]){
		// creat a new sphere x distance from point in oldDir

		if(!projectile_exists){
		glm::vec3 entityPosition = glm::vec3(entityCubeModelMatrix[3]);
		glm::vec3 newSpherePos = ( (2.0f*oldDir) + entityPosition );
		//glm::vec3 new_v = glm::vec3(glm::translate(new_sphere_pos) * glm::vec4(new_sphere_pos, 1.0f));


		glm::mat4 new_SphereModelMatrix = glm::mat4(1.0f);
		new_SphereModelMatrix = glm::translate(glm::vec3(newSpherePos.x, 0.557555f, newSpherePos.z)) * new_SphereModelMatrix;
		
		// maby have a set/add new projectile function...
		Entity& newProjectile = ProjectileList[0];
		newProjectile.modelMatrix = new_SphereModelMatrix;
		newProjectile.collider[0] = newSpherePos;
		newProjectile.dir = oldDir;
		newProjectile.active = true;
		//ProjectileList.emplace_back(Entity{testSphereCollider,new_SphereModelMatrix,oldDir,true});
		projectile_exists = true;
		}
		
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
		glm::vec3 entityPosition = glm::vec3(entityCubeModelMatrix[3]);
		glm::vec3 enemyPosition = glm::vec3(enemyCubeModelMatrix[3]);
		glm::vec3 enemyDir = normalize(entityPosition - enemyPosition);

		float enemySpeed = 10.0f;
		enemyCubeModelMatrix = glm::translate( enemyDir * enemySpeed * deltaTime) * enemyCubeModelMatrix;

		//for(glm::vec3& v: enemyCubeCollider.vertices){
		for(glm::vec3& v: ColliderList.back()){
		//v += glm::vec3(0.0f,1.0f, 0.0f); // motsvarar translation

			glm::vec3 new_v = glm::vec3(glm::translate( enemyDir * enemySpeed * deltaTime) * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
		}

	//std::vector<ConvexCollider> ColliderList;
	//std::vector<glm::vec3> entityCollider;
	glm::vec3 responsVector = glm::vec3(0.0f);	
	for(std::vector<glm::vec3>& c: ColliderList){
		//printf("c: %zu \n",c.vertices.size());
		
		if(col::collision(entityCollider, c, responsVector)){
			entityCubeModelMatrix = glm::translate(responsVector) * entityCubeModelMatrix; // should glm::translate(responsVector*deltaTime)?

			for(glm::vec3& v: entityCollider){
			glm::vec3 new_v = glm::vec3(glm::translate(responsVector) * glm::vec4(v, 1.0f));
			v = glm::vec3(new_v.x,0.0f,new_v.z);
			}

		}
		
	}
}

 
void projectile_collision_resolution(std::vector<Entity>& entitys){

	// infuture we will only lett the player have a limited number of projectiles 
	glm::vec3 responsVector = glm::vec3(0.0f);
	for(Entity& e: entitys)
		if(e.active == true) 
			for(std::vector<glm::vec3>& c: ColliderList)
				if(col::collision(e.collider, c, responsVector)){

					projectile_exists= false;
					e.active = false;
					//printf("projectile collided");
				}
}

void projectile_collision_resolution(){
	// infuture we will only lett the player have a limited number of projectiles 
	glm::vec3 responsVector = glm::vec3(0.0f);
	for(Entity& e: ProjectileList)
		if(e.active == true)
			for(std::vector<glm::vec3>& c: ColliderList)
				if(col::collision(e.collider, c, responsVector)){
					e.move_entity_circle(glm::translate(responsVector));
					e.dir = glm::reflect(e.dir, glm::normalize(responsVector));
					if ( bounce_counter++ == 2){
						projectile_exists= false;
						e.active = false;
						bounce_counter = 0;
					}
					//printf("projectile collided");
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

		for(Entity& p: ProjectileList){
			if(p.active == true)
				p.move_entity_circle(translate(30.0f*p.dir*deltaTime));
		}

		//projectile_collision_resolution(ProjectileList);
		projectile_collision_resolution();
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
	labhelper::freeModel(entityCubeModel);
	labhelper::freeModel(testSphereModel);

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
