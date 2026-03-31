#include "Model.h"
#include "labhelper.h"
#include <iostream>
#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include <tiny_obj_loader.h>
//#include <experimental/tinyobj_loader_opt.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <GL/glew.h>
#include <stb_image.h>

namespace labhelper
{
bool Texture::load(const std::string& _directory, const std::string& _filename, int _components)
{
	filename = _filename;
	directory = _directory;
	valid = true;
	int components;
	data = stbi_load((directory + filename).c_str(), &width, &height, &components, _components);
	if(data == nullptr)
	{
		std::cout << "ERROR: loadModelFromOBJ(): Failed to load texture: " << filename << " in " << _directory
		          << "\n";
		exit(1);
	}
	glGenTextures(1, &gl_id);
	glBindTexture(GL_TEXTURE_2D, gl_id);
	GLenum format, internal_format;
	if(_components == 1)
	{
		format = GL_R;
		internal_format = GL_R8;
	}
	else if(_components == 3)
	{
		format = GL_RGB;
		internal_format = GL_RGB;
	}
	else if(_components == 4)
	{
		format = GL_RGBA;
		internal_format = GL_RGBA;
	}
	else
	{
		std::cout << "Texture loading not implemented for this number of compenents.\n";
		exit(1);
	}
	glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 16);

	glBindTexture( GL_TEXTURE_2D, 0 );
	return true;
}

///////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////
Model::~Model()
{
	for(auto& material : m_materials)
	{
		if(material.m_color_texture.valid)
			glDeleteTextures(1, &material.m_color_texture.gl_id);
		if(material.m_reflectivity_texture.valid)
			glDeleteTextures(1, &material.m_reflectivity_texture.gl_id);
		if(material.m_shininess_texture.valid)
			glDeleteTextures(1, &material.m_shininess_texture.gl_id);
		if(material.m_metalness_texture.valid)
			glDeleteTextures(1, &material.m_metalness_texture.gl_id);
		if(material.m_fresnel_texture.valid)
			glDeleteTextures(1, &material.m_fresnel_texture.gl_id);
		if(material.m_emission_texture.valid)
			glDeleteTextures(1, &material.m_emission_texture.gl_id);
	}
	glDeleteBuffers(1, &m_positions_bo);
	glDeleteBuffers(1, &m_normals_bo);
	glDeleteBuffers(1, &m_texture_coordinates_bo);
}

Model* loadModelFromOBJ(std::string path)
{
	///////////////////////////////////////////////////////////////////////
	// Separate filename into directory, base filename and extension
	// NOTE: This can be made a LOT simpler as soon as compilers properly
	//		 support std::filesystem (C++17)
	///////////////////////////////////////////////////////////////////////
	size_t separator = path.find_last_of("\\/");
	std::string filename, extension, directory;
	if(separator != std::string::npos)
	{
		filename = path.substr(separator + 1, path.size() - separator - 1);
		directory = path.substr(0, separator + 1);
	}
	else
	{
		filename = path;
		directory = "./";
	}
	separator = filename.find_last_of(".");
	if(separator == std::string::npos)
	{
		std::cout << "Fatal: loadModelFromOBJ(): Expecting filename ending in '.obj'\n";
		exit(1);
	}
	extension = filename.substr(separator, filename.size() - separator);
	filename = filename.substr(0, separator);

	///////////////////////////////////////////////////////////////////////
	// Parse the OBJ file using tinyobj
	///////////////////////////////////////////////////////////////////////
	std::cout << "Loading " << path << "..." << std::flush;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	// Expect '.mtl' file in the same directory and triangulate meshes
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
	                            (directory + filename + extension).c_str(), directory.c_str(), true);
	if(!err.empty())
	{ // `err` may contain warning message.
		std::cerr << err << std::endl;
	}
	if(!ret)
	{
		exit(1);
	}
	Model* model = new Model;
	model->m_name = filename;
	model->m_filename = path;

	///////////////////////////////////////////////////////////////////////
	// Transform all materials into our datastructure
	///////////////////////////////////////////////////////////////////////
	for(const auto& m : materials)
	{
		Material material;
		material.m_name = m.name;
		material.m_color = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
		if(m.diffuse_texname != "")
		{
			material.m_color_texture.load(directory, m.diffuse_texname, 4);
		}
		material.m_reflectivity = m.specular[0];
		if(m.specular_texname != "")
		{
			material.m_reflectivity_texture.load(directory, m.specular_texname, 1);
		}
		material.m_metalness = m.metallic;
		if(m.metallic_texname != "")
		{
			material.m_metalness_texture.load(directory, m.metallic_texname, 1);
		}
		material.m_fresnel = m.sheen;
		if(m.sheen_texname != "")
		{
			material.m_fresnel_texture.load(directory, m.sheen_texname, 1);
		}
		material.m_shininess = m.roughness;
		if(m.roughness_texname != "")
		{
			material.m_shininess_texture.load(directory, m.roughness_texname, 1);
		}
		material.m_emission = m.emission[0];
		if(m.emissive_texname != "")
		{
			material.m_emission_texture.load(directory, m.emissive_texname, 4);
		}
		material.m_transparency = m.transmittance[0];
		model->m_materials.push_back(material);
	}

	///////////////////////////////////////////////////////////////////////
	// A vertex in the OBJ file may have different indices for position,
	// normal and texture coordinate. We will not even attempt to use
	// indexed lookups, but will store a simple vertex stream per mesh.
	///////////////////////////////////////////////////////////////////////
	uint64_t number_of_vertices = 0;
	for(const auto& shape : shapes)
	{
		number_of_vertices += shape.mesh.indices.size();
	}
	model->m_positions.resize(number_of_vertices);
	model->m_normals.resize(number_of_vertices);
	model->m_texture_coordinates.resize(number_of_vertices);

	///////////////////////////////////////////////////////////////////////
	// For each vertex _position_ auto generate a normal that will be used
	// if no normal is supplied.
	///////////////////////////////////////////////////////////////////////
	std::vector<glm::vec4> auto_normals(attrib.vertices.size() / 3);
	for(const auto& shape : shapes)
	{
		for(int face = 0; face < int(shape.mesh.indices.size()) / 3; face++)
		{
			glm::vec3 v0 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 2]);
			glm::vec3 v1 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 2]);
			glm::vec3 v2 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 2]);

			glm::vec3 e0 = glm::normalize(v1 - v0);
			glm::vec3 e1 = glm::normalize(v2 - v0);
			glm::vec3 face_normal = cross(e0, e1);

			auto_normals[shape.mesh.indices[face * 3 + 0].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 1].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 2].vertex_index] += glm::vec4(face_normal, 1.0f);
		}
	}
	for(auto& normal : auto_normals)
	{
		normal = (1.0f / normal.w) * normal;
	}

	///////////////////////////////////////////////////////////////////////
	// Now we will turn all shapes into Meshes. A shape that has several
	// materials will be split into several meshes with unique names
	///////////////////////////////////////////////////////////////////////
	int vertices_so_far = 0;
	for(const auto& shape : shapes)
	{
		///////////////////////////////////////////////////////////////////
		// The shapes in an OBJ file may several different materials.
		// If so, we will split the shape into one Mesh per Material
		///////////////////////////////////////////////////////////////////
		int next_material_index = shape.mesh.material_ids[0];
		int next_material_starting_face = 0;
		std::vector<bool> finished_materials(materials.size(), false);
		int number_of_materials_in_shape = 0;
		while(next_material_index != -1)
		{
			int current_material_index = next_material_index;
			int current_material_starting_face = next_material_starting_face;
			next_material_index = -1;
			next_material_starting_face = -1;
			// Process a new Mesh with a unique material
			Mesh mesh;
			mesh.m_name = shape.name + "_" + materials[current_material_index].name;
			mesh.m_material_idx = current_material_index;
			mesh.m_start_index = vertices_so_far;
			number_of_materials_in_shape += 1;

			uint64_t number_of_faces = shape.mesh.indices.size() / 3;
			for(int i = current_material_starting_face; i < number_of_faces; i++)
			{
				if(shape.mesh.material_ids[i] != current_material_index)
				{
					if(next_material_index >= 0)
						continue;
					else if(finished_materials[shape.mesh.material_ids[i]])
						continue;
					else
					{ // Found a new material that we have not processed.
						next_material_index = shape.mesh.material_ids[i];
						next_material_starting_face = i;
					}
				}
				else
				{
					///////////////////////////////////////////////////////
					// Now we generate the vertices
					///////////////////////////////////////////////////////
					for(int j = 0; j < 3; j++)
					{
						int v = shape.mesh.indices[i * 3 + j].vertex_index;
						model->m_positions[vertices_so_far + j] =
						    glm::vec3(attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 0],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 1],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 2]);
						if(shape.mesh.indices[i * 3 + j].normal_index == -1)
						{
							// No normal, use the autogenerated
							model->m_normals[vertices_so_far + j] = glm::vec3(
							    auto_normals[shape.mesh.indices[i * 3 + j].vertex_index]);
						}
						else
						{
							model->m_normals[vertices_so_far + j] =
							    glm::vec3(attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 0],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 1],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 2]);
						}
						if(shape.mesh.indices[i * 3 + j].texcoord_index == -1)
						{
							// No UV coordinates. Use null.
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(0.0f);
						}
						else
						{
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 0],
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 1]);
						}
					}
					vertices_so_far += 3;
				}
			}
			///////////////////////////////////////////////////////////////
			// Finalize and push this mesh to the list
			///////////////////////////////////////////////////////////////
			mesh.m_number_of_vertices = vertices_so_far - mesh.m_start_index;
			model->m_meshes.push_back(mesh);
			finished_materials[current_material_index] = true;
		}
		if(number_of_materials_in_shape == 1)
		{
			model->m_meshes.back().m_name = shape.name;
		}
	}

	///////////////////////////////////////////////////////////////////////
	// Upload to GPU
	///////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &model->m_vaob);
	glBindVertexArray(model->m_vaob);
	glGenBuffers(1, &model->m_positions_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_positions_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_positions.size() * sizeof(glm::vec3), &model->m_positions[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);
	glGenBuffers(1, &model->m_normals_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_normals_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_normals.size() * sizeof(glm::vec3), &model->m_normals[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);
	glGenBuffers(1, &model->m_texture_coordinates_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_texture_coordinates_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_texture_coordinates.size() * sizeof(glm::vec2),
	             &model->m_texture_coordinates[0].x, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindVertexArray( 0 );
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	std::cout << "done.\n";
	return model;
}

/* we want to have the indices associated with the outer edges of the vertices that have y == 0.
 this means we need to exclude all vertices that only has edges that are shared by other faces(triangles), 
 this means we need will need to find all edges that are shared by triangles 
 so because we will need these edges to calculate the "normals"/"projection axices" we will just store all edges now....... 
*/

// helpfunction
/*
// if i check this before storing edges i will lose the ordering of the edges
bool is_in(std::vector<glm::vec3>&& vertices2d, glm::vec3&& v1, glm::vec3&& v2, glm::vec3&& v3){

	return false;
}


	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	//std::vector<tinyobj::material_t> materials; might be funn to use the material propperties to define non- or elastic collision..

	*/

void printEdges(std::vector<std::pair<uint32_t, uint32_t>>& edges, tinyobj::attrib_t& attrib, uint32_t offset){

	//if (offset > edges.size()) dont think this is needed, i < edges.size(); should be checked right after
		
		for(uint32_t i = offset; i < edges.size(); i++){
			int idx1 = edges[i].first;
			int idx2 = edges[i].second;
			printf("%i : Edge { %i , %i }, { %f, %f , %f },{ %f, %f , %f }  \n", 
				i, idx1, idx2 ,
				 attrib.vertices[idx1],attrib.vertices[idx1+1],attrib.vertices[idx1+2],
				 attrib.vertices[idx2],attrib.vertices[idx2+1],attrib.vertices[idx2+2]);
		}
		printf(" __________________ \n");
}
// this one is probably more troble than first one....
void Get_2dEdgeVertices_of_convexShapeTinyobj(tinyobj::attrib_t& attrib,std::vector<tinyobj::shape_t>& shapes){

	std::vector<glm::vec3> allVerticePos(attrib.vertices.begin(),attrib.vertices.end());

	for(const tinyobj::shape_t& shape: shapes){

		std::vector<std::pair<uint32_t, uint32_t>> local_edges;// will need to resize to "slim fit" the data, i hope that .size does not 
		local_edges.reserve(shape.mesh.indices.size());

		for(int f = 0; f < int(shape.mesh.indices.size()) / 3; f++){

			// why do i need to multiply the indices with three here????????
			// kanske för att de andra attributen ligger i mellan.....?
			uint32_t v1_idx  = shape.mesh.indices[f*3].vertex_index*3;
			uint32_t v2_idx  = shape.mesh.indices[(f*3)+1].vertex_index*3;
			uint32_t v3_idx  = shape.mesh.indices[(f*3)+2].vertex_index*3;

			//printf("\n indices: %i , %i , %i",v1_idx,v2_idx,v3_idx );
			glm::vec3 v1  = glm::vec3(attrib.vertices[v1_idx],attrib.vertices[v1_idx+1],attrib.vertices[v1_idx+2]);
			glm::vec3 v2  = glm::vec3(attrib.vertices[v2_idx],attrib.vertices[v2_idx+1],attrib.vertices[v2_idx+2]);
			glm::vec3 v3  = glm::vec3(attrib.vertices[v3_idx],attrib.vertices[v3_idx+1],attrib.vertices[v3_idx+2]);

			//glm::vec3 testnoll= glm::vec3(attrib.vertices[0],attrib.vertices[1],attrib.vertices[2]);
		//printf("\n v1: { %f, %f , %f }",v1.x,v1.y,v1.z);
		//printf("\n v2: { %f, %f , %f }",v2.x,v2.y,v2.z);
		//printf("\n v3: { %f, %f , %f }",v3.x,v3.y,v3.z);
			if (v1.y != 0 || v2.y != 0 ||v3.y != 0 )
				continue;

			local_edges.emplace_back(v1_idx,v2_idx);
			local_edges.emplace_back(v2_idx,v3_idx);
			local_edges.emplace_back(v3_idx,v1_idx);
		}
		printf("\n Before outline stuff mesh name:%s \n", shape.name.c_str());
		printEdges(local_edges,attrib,0);
		// should i store all vertex indices so that they are in one vector or should i 

		uint32_t outline_index = 0;
		// now I need to find all edges that are shared between vertices:
		for(uint32_t i = outline_index; i < local_edges.size(); i++){

			std::pair<uint32_t, uint32_t>& a = local_edges[i];
			
			//for each triangle check if edges are(xi,yi) == (xt,yt) or (xi,yi) == (yt, xt)
			// for now we are just wapping all 

			uint32_t swap_counter = 0;
			for(uint32_t j = i+1; j < local_edges.size(); j++){
				//if(lambda(local_edges[j])){

				std::pair<uint32_t, uint32_t>& b = local_edges[j];
				if( (a.first == b.first && a.second == b.second) || 
					(a.first == b.second && a.second == b.first)) {
					std::swap(local_edges[swap_counter++], local_edges[j]);
				}
			}
			if (swap_counter > outline_index){ 
				std::swap(local_edges[swap_counter++], local_edges[i]);
				outline_index = swap_counter;
			}
		}
		printf(" \n After outline stuff mesh name: %s \n", shape.name.c_str());
		printEdges(local_edges,attrib,outline_index);

	}


}

Model* loadModelFromOBJ(std::string path, bool test)
{
	///////////////////////////////////////////////////////////////////////
	// Separate filename into directory, base filename and extension
	// NOTE: This can be made a LOT simpler as soon as compilers properly
	//		 support std::filesystem (C++17)
	///////////////////////////////////////////////////////////////////////
	size_t separator = path.find_last_of("\\/");
	std::string filename, extension, directory;
	if(separator != std::string::npos)
	{
		filename = path.substr(separator + 1, path.size() - separator - 1);
		directory = path.substr(0, separator + 1);
	}
	else
	{
		filename = path;
		directory = "./";
	}
	separator = filename.find_last_of(".");
	if(separator == std::string::npos)
	{
		std::cout << "Fatal: loadModelFromOBJ(): Expecting filename ending in '.obj'\n";
		exit(1);
	}
	extension = filename.substr(separator, filename.size() - separator);
	filename = filename.substr(0, separator);

	///////////////////////////////////////////////////////////////////////
	// Parse the OBJ file using tinyobj
	///////////////////////////////////////////////////////////////////////
	std::cout << "Loading " << path << "..." << std::flush;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	// Expect '.mtl' file in the same directory and triangulate meshes
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
	                            (directory + filename + extension).c_str(), directory.c_str(), true);
	if(!err.empty())
	{ // `err` may contain warning message.
		std::cerr << err << std::endl;
	}
	if(!ret)
	{
		exit(1);
	}
	Get_2dEdgeVertices_of_convexShapeTinyobj(attrib,shapes);
	Model* model = new Model;
	model->m_name = filename;
	model->m_filename = path;

	///////////////////////////////////////////////////////////////////////
	// Transform all materials into our datastructure
	///////////////////////////////////////////////////////////////////////
	for(const auto& m : materials)
	{
		Material material;
		material.m_name = m.name;
		material.m_color = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
		if(m.diffuse_texname != "")
		{
			material.m_color_texture.load(directory, m.diffuse_texname, 4);
		}
		material.m_reflectivity = m.specular[0];
		if(m.specular_texname != "")
		{
			material.m_reflectivity_texture.load(directory, m.specular_texname, 1);
		}
		material.m_metalness = m.metallic;
		if(m.metallic_texname != "")
		{
			material.m_metalness_texture.load(directory, m.metallic_texname, 1);
		}
		material.m_fresnel = m.sheen;
		if(m.sheen_texname != "")
		{
			material.m_fresnel_texture.load(directory, m.sheen_texname, 1);
		}
		material.m_shininess = m.roughness;
		if(m.roughness_texname != "")
		{
			material.m_shininess_texture.load(directory, m.roughness_texname, 1);
		}
		material.m_emission = m.emission[0];
		if(m.emissive_texname != "")
		{
			material.m_emission_texture.load(directory, m.emissive_texname, 4);
		}
		material.m_transparency = m.transmittance[0];
		model->m_materials.push_back(material);
	}

	///////////////////////////////////////////////////////////////////////
	// A vertex in the OBJ file may have different indices for position,
	// normal and texture coordinate. We will not even attempt to use
	// indexed lookups, but will store a simple vertex stream per mesh.
	///////////////////////////////////////////////////////////////////////
	uint64_t number_of_vertices = 0;
	for(const auto& shape : shapes)
	{
		number_of_vertices += shape.mesh.indices.size();
	}
	model->m_positions.resize(number_of_vertices);
	model->m_normals.resize(number_of_vertices);
	model->m_texture_coordinates.resize(number_of_vertices);

	///////////////////////////////////////////////////////////////////////
	// For each vertex _position_ auto generate a normal that will be used
	// if no normal is supplied.
	///////////////////////////////////////////////////////////////////////
	std::vector<glm::vec4> auto_normals(attrib.vertices.size() / 3);
	for(const auto& shape : shapes)
	{
		for(int face = 0; face < int(shape.mesh.indices.size()) / 3; face++)
		{
			glm::vec3 v0 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 2]);
			glm::vec3 v1 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 2]);
			glm::vec3 v2 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 2]);

			glm::vec3 e0 = glm::normalize(v1 - v0);
			glm::vec3 e1 = glm::normalize(v2 - v0);
			glm::vec3 face_normal = cross(e0, e1);

			auto_normals[shape.mesh.indices[face * 3 + 0].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 1].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 2].vertex_index] += glm::vec4(face_normal, 1.0f);
		}
	}
	for(auto& normal : auto_normals)
	{
		normal = (1.0f / normal.w) * normal;
	}

	///////////////////////////////////////////////////////////////////////
	// Now we will turn all shapes into Meshes. A shape that has several
	// materials will be split into several meshes with unique names
	///////////////////////////////////////////////////////////////////////
	int vertices_so_far = 0;
	for(const auto& shape : shapes)
	{
		///////////////////////////////////////////////////////////////////
		// The shapes in an OBJ file may several different materials.
		// If so, we will split the shape into one Mesh per Material
		///////////////////////////////////////////////////////////////////
		int next_material_index = shape.mesh.material_ids[0];
		int next_material_starting_face = 0;
		std::vector<bool> finished_materials(materials.size(), false);
		int number_of_materials_in_shape = 0;
		while(next_material_index != -1)
		{
			int current_material_index = next_material_index;
			int current_material_starting_face = next_material_starting_face;
			next_material_index = -1;
			next_material_starting_face = -1;
			// Process a new Mesh with a unique material
			Mesh mesh;
			mesh.m_name = shape.name + "_" + materials[current_material_index].name;
			mesh.m_material_idx = current_material_index;
			mesh.m_start_index = vertices_so_far;
			number_of_materials_in_shape += 1;

			uint64_t number_of_faces = shape.mesh.indices.size() / 3;
			for(int i = current_material_starting_face; i < number_of_faces; i++)
			{
				if(shape.mesh.material_ids[i] != current_material_index)
				{
					if(next_material_index >= 0)
						continue;
					else if(finished_materials[shape.mesh.material_ids[i]])
						continue;
					else
					{ // Found a new material that we have not processed.
						next_material_index = shape.mesh.material_ids[i];
						next_material_starting_face = i;
					}
				}
				else
				{
					///////////////////////////////////////////////////////
					// Now we generate the vertices
					///////////////////////////////////////////////////////
					for(int j = 0; j < 3; j++)
					{
						int v = shape.mesh.indices[i * 3 + j].vertex_index;
						model->m_positions[vertices_so_far + j] =
						    glm::vec3(attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 0],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 1],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 2]);
						if(shape.mesh.indices[i * 3 + j].normal_index == -1)
						{
							// No normal, use the autogenerated
							model->m_normals[vertices_so_far + j] = glm::vec3(
							    auto_normals[shape.mesh.indices[i * 3 + j].vertex_index]);
						}
						else
						{
							model->m_normals[vertices_so_far + j] =
							    glm::vec3(attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 0],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 1],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 2]);
						}
						if(shape.mesh.indices[i * 3 + j].texcoord_index == -1)
						{
							// No UV coordinates. Use null.
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(0.0f);
						}
						else
						{
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 0],
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 1]);
						}
					}
					vertices_so_far += 3;
				}
			}
			///////////////////////////////////////////////////////////////
			// Finalize and push this mesh to the list
			///////////////////////////////////////////////////////////////
			mesh.m_number_of_vertices = vertices_so_far - mesh.m_start_index;
			model->m_meshes.push_back(mesh);
			finished_materials[current_material_index] = true;
		}
		if(number_of_materials_in_shape == 1)
		{
			model->m_meshes.back().m_name = shape.name;
		}
	}

	///////////////////////////////////////////////////////////////////////
	// Upload to GPU
	///////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &model->m_vaob);
	glBindVertexArray(model->m_vaob);
	glGenBuffers(1, &model->m_positions_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_positions_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_positions.size() * sizeof(glm::vec3), &model->m_positions[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);
	glGenBuffers(1, &model->m_normals_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_normals_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_normals.size() * sizeof(glm::vec3), &model->m_normals[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);
	glGenBuffers(1, &model->m_texture_coordinates_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_texture_coordinates_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_texture_coordinates.size() * sizeof(glm::vec2),
	             &model->m_texture_coordinates[0].x, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindVertexArray( 0 );
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	std::cout << "done.\n";
	return model;
}



void Get_2dEdgeVertices_of_convexShapeModel(Model& model2dConvexShape){

	// could just use std::array<uint32_t,2>
	struct Edge{
		uint32_t idx1;
		uint32_t idx2;
	};

	// should probably return
	struct ColliderSAT2D{
		std::string m_name;
		std::vector<Edge> Edges;
		std::vector<glm::vec3> vertices; // we keep them in 3D to easily apply transforms, or is this stupid and I should just use vec2?
	};

	std::vector<glm::vec3> allVerticePos(model2dConvexShape.m_positions);

	// there are 3 vert in one triangle and 3 edges
	std::vector<Edge> allEdges(model2dConvexShape.m_positions.size()/2); // will need to resize to "slim fit" the data

	// vectices2d should be mutch smaller than all vert epecially because it shouldn't contain douplicate vert witch m_pos does
	std::vector<glm::vec3> vertices2d(model2dConvexShape.m_positions.size()/2); // will need to resize to "slim fit" the data

	std::vector<glm::vec3> local_vert;
	local_vert.reserve(model2dConvexShape.m_positions.size()/2);

	//for(const Mesh& msh: model2dConvexShape.m_meshes){
	const Mesh& msh = model2dConvexShape.m_meshes[2];

		uint32_t off = msh.m_start_index;
		uint32_t end = msh.m_number_of_vertices + off;
		std::vector<Edge> local_edges;// will need to resize to "slim fit" the data, i hope that .size does not 
		
		local_edges.reserve(msh.m_number_of_vertices/2);
		uint32_t edge_ind = 0;
		for(uint32_t i = off; i < end; i+=3 ){

			// get vertices of triangle
			glm::vec3 v1 = allVerticePos[i];
			glm::vec3 v2 = allVerticePos[i+1];
			glm::vec3 v3 = allVerticePos[i+2];

			// we should only care for vertices that are on the "2d pane"
			if (v1.y != 0 || v2.y != 0 ||v3.y != 0 )
				continue;

			if(local_vert.size() == 0){
				local_vert.emplace_back(v1);
				local_vert.emplace_back(v2);
				local_vert.emplace_back(v3);

				local_edges.emplace_back(Edge{1,2});
				local_edges.emplace_back(Edge{2,3});
				local_edges.emplace_back(Edge{3,1});
			}

			//for()
				
			
			// store all edges for the triangles where all the vertices have y == 0, aka the 2d triangles
			local_edges.emplace_back(Edge{i,i+1});
			local_edges.emplace_back(Edge{i+1,i+2});
			local_edges.emplace_back(Edge{i+2,i});
		}
		
		for(uint32_t i = 0; i < local_edges.size(); i+=3){

			for(uint32_t j = i+3; j < local_edges.size(); j+=3){
			//for(uint32_t j = local_edges.size(); j > i + 1; j--){


			if (local_edges[i].idx1 == local_edges[j].idx1)
				continue;

			if(allVerticePos[local_edges[i].idx1] != allVerticePos[local_edges[j].idx1]){

			}
			}
		}
		

		printf(" Before outline stuff mesh name: %s \n", msh.m_name.c_str());
		for(uint32_t i = 0; i < local_edges.size(); i++){
			int idx1 = local_edges[i].idx1;
			int idx2 = local_edges[i].idx2;
			printf("%i : Edge { %i , %i }, { %f, %f , %f },{ %f, %f , %f }  \n", 
				i, idx1, idx2 ,
				allVerticePos[idx1].x,allVerticePos[idx1].y,allVerticePos[idx1].z,
				allVerticePos[idx2].x,allVerticePos[idx2].y,allVerticePos[idx2].z );
		}
		printf(" __________________ \n");
		ColliderSAT2D mesh_colider{};
		uint32_t outline_index = 0;
		// now I need to find all edges that are shared between vertices:
		for(uint32_t i = outline_index; i < local_edges.size(); i++){

			Edge& a = local_edges[i];
			
			//for each triangle check if edges are(xi,yi) == (xt,yt) or (xi,yi) == (yt, xt)
			// for now we are just wapping all 

			uint32_t swap_counter = 0;
			for(uint32_t j = i+1; j < local_edges.size(); j++){
				//if(lambda(local_edges[j])){

				Edge& b = local_edges[j];
				if( (a.idx1 == b.idx1 && a.idx2 == b.idx2) || 
					(a.idx1 == b.idx2 && a.idx2 == b.idx1)) {
					std::swap(local_edges[swap_counter++], local_edges[j]);
				}
			}
			if (swap_counter > outline_index){ 
				std::swap(local_edges[swap_counter++], local_edges[i]);
				outline_index = swap_counter;
			}
		}
		// now from outline_index and up the local edges only contain the outer edges
		printf("mesh name: %s \n", msh.m_name.c_str());
		for(uint32_t i = outline_index; i < local_edges.size(); i++){
			int idx1 = local_edges[i].idx1;
			int idx2 = local_edges[i].idx2;
			printf("%i : Edge { %i , %i }, { %f, %f , %f },{ %f, %f , %f }  \n", 
				i - outline_index, idx1, idx2 ,
				allVerticePos[idx1].x,allVerticePos[idx1].y,allVerticePos[idx1].z,
				allVerticePos[idx2].x,allVerticePos[idx2].y,allVerticePos[idx2].z );
		}
			

		// we want to order the edges so that they have a chain... Edge{4,6}, Edge{6,8}, Edge{8,10}, Edge{10,1}, Edge{1,4}
		// or even better if we could just definde whats/what normals are inside and outside of the shape... Or dose it make a difference???
		/*
		Edge comp_edge = local_edges[outline_index];
		for(uint32_t i = outline_index; i < local_edges.size(); i++){

			// find the edge with the matching right vertex
			for(uint32_t j = i+1; j < local_edges.size(); j++){  // will this lead to of by one or sime thing...

				if(comp_edge.idx2 == local_edges[j].idx1) {
					//std::swap(local_edges[i+1], local_edges[j]);
					comp_edge = local_edges[j]; // i don't actualy need to swap the edges, i don't need them in order but it makes it easier for the loop because i don't want to revisit
					break;
				}

				if(comp_edge.idx2 == local_edges[j].idx2){
					std::swap(local_edges[j].idx1, local_edges[j].idx2);
					comp_edge = local_edges[j];
				}
				// if comp_edge is never uppdated then we should cast an error because then the edges from previus "cleaning" of data is wrong.. 
				continue;
			}

		}
		*/
		/*
			Edge y = local_edges[0]; 
			std::vector<Edge>::iterator test;
			auto lambda = [=] (Edge& x) -> bool { return (x.idx1 == y.idx1 && x.idx2 == y.idx2) || (x.idx1 == y.idx2 && x.idx2 == y.idx1);};
			test = std::remove_if(local_edges.begin(),local_edges.end(),lambda);
		while(true){ // while test 
			//Edge y = local_edges[i]; // does this work with no copy constructor...
			
			//std::remove_if returns an iterator that is at the  
			test = std::remove_if(local_edges.begin(),local_edges.end(),lambda);

		}
		*/
	//}

}

// probably will need to change render function for models also
Model* Use_lookup_loadModelFromOBJ(std::string path)
{
	///////////////////////////////////////////////////////////////////////
	// Separate filename into directory, base filename and extension
	// NOTE: This can be made a LOT simpler as soon as compilers properly
	//		 support std::filesystem (C++17)
	///////////////////////////////////////////////////////////////////////
	size_t separator = path.find_last_of("\\/");
	std::string filename, extension, directory;
	if(separator != std::string::npos)
	{
		filename = path.substr(separator + 1, path.size() - separator - 1);
		directory = path.substr(0, separator + 1);
	}
	else
	{
		filename = path;
		directory = "./";
	}
	separator = filename.find_last_of(".");
	if(separator == std::string::npos)
	{
		std::cout << "Fatal: loadModelFromOBJ(): Expecting filename ending in '.obj'\n";
		exit(1);
	}
	extension = filename.substr(separator, filename.size() - separator);
	filename = filename.substr(0, separator);

	///////////////////////////////////////////////////////////////////////
	// Parse the OBJ file using tinyobj
	///////////////////////////////////////////////////////////////////////
	std::cout << "Loading " << path << "..." << std::flush;
	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;
	// Expect '.mtl' file in the same directory and triangulate meshes
	bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err,
	                            (directory + filename + extension).c_str(), directory.c_str(), true);
	if(!err.empty())
	{ // `err` may contain warning message.
		std::cerr << err << std::endl;
	}
	if(!ret)
	{
		exit(1);
	}
	Model* model = new Model;
	model->m_name = filename;
	model->m_filename = path;

	///////////////////////////////////////////////////////////////////////
	// Transform all materials into our datastructure
	///////////////////////////////////////////////////////////////////////
	for(const auto& m : materials)
	{
		Material material;
		material.m_name = m.name;
		material.m_color = glm::vec3(m.diffuse[0], m.diffuse[1], m.diffuse[2]);
		if(m.diffuse_texname != "")
		{
			material.m_color_texture.load(directory, m.diffuse_texname, 4);
		}
		material.m_reflectivity = m.specular[0];
		if(m.specular_texname != "")
		{
			material.m_reflectivity_texture.load(directory, m.specular_texname, 1);
		}
		material.m_metalness = m.metallic;
		if(m.metallic_texname != "")
		{
			material.m_metalness_texture.load(directory, m.metallic_texname, 1);
		}
		material.m_fresnel = m.sheen;
		if(m.sheen_texname != "")
		{
			material.m_fresnel_texture.load(directory, m.sheen_texname, 1);
		}
		material.m_shininess = m.roughness;
		if(m.roughness_texname != "")
		{
			material.m_shininess_texture.load(directory, m.roughness_texname, 1);
		}
		material.m_emission = m.emission[0];
		if(m.emissive_texname != "")
		{
			material.m_emission_texture.load(directory, m.emissive_texname, 4);
		}
		material.m_transparency = m.transmittance[0];
		model->m_materials.push_back(material);
	}

	///////////////////////////////////////////////////////////////////////
	// A vertex in the OBJ file may have different indices for position,
	// normal and texture coordinate. We will not even attempt to use
	// indexed lookups, but will store a simple vertex stream per mesh.
	///////////////////////////////////////////////////////////////////////
	uint64_t number_of_vertices = 0;
	for(const auto& shape : shapes)
	{
		number_of_vertices += shape.mesh.indices.size();
	}
	model->m_positions.resize(number_of_vertices);
	model->m_normals.resize(number_of_vertices);
	model->m_texture_coordinates.resize(number_of_vertices);

	///////////////////////////////////////////////////////////////////////
	// For each vertex _position_ auto generate a normal that will be used
	// if no normal is supplied.
	///////////////////////////////////////////////////////////////////////
	std::vector<glm::vec4> auto_normals(attrib.vertices.size() / 3);
	for(const auto& shape : shapes)
	{
		for(int face = 0; face < int(shape.mesh.indices.size()) / 3; face++)
		{
			glm::vec3 v0 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 0].vertex_index * 3 + 2]);
			glm::vec3 v1 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 1].vertex_index * 3 + 2]);
			glm::vec3 v2 = glm::vec3(attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 0],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 1],
			                         attrib.vertices[shape.mesh.indices[face * 3 + 2].vertex_index * 3 + 2]);

			glm::vec3 e0 = glm::normalize(v1 - v0);
			glm::vec3 e1 = glm::normalize(v2 - v0);
			glm::vec3 face_normal = cross(e0, e1);

			auto_normals[shape.mesh.indices[face * 3 + 0].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 1].vertex_index] += glm::vec4(face_normal, 1.0f);
			auto_normals[shape.mesh.indices[face * 3 + 2].vertex_index] += glm::vec4(face_normal, 1.0f);
		}
	}
	for(auto& normal : auto_normals)
	{
		normal = (1.0f / normal.w) * normal;
	}

	///////////////////////////////////////////////////////////////////////
	// Now we will turn all shapes into Meshes. A shape that has several
	// materials will be split into several meshes with unique names
	///////////////////////////////////////////////////////////////////////
	int vertices_so_far = 0;
	for(const auto& shape : shapes)
	{
		///////////////////////////////////////////////////////////////////
		// The shapes in an OBJ file may several different materials.
		// If so, we will split the shape into one Mesh per Material
		///////////////////////////////////////////////////////////////////
		int next_material_index = shape.mesh.material_ids[0];
		int next_material_starting_face = 0;
		std::vector<bool> finished_materials(materials.size(), false);
		int number_of_materials_in_shape = 0;
		while(next_material_index != -1)
		{
			int current_material_index = next_material_index;
			int current_material_starting_face = next_material_starting_face;
			next_material_index = -1;
			next_material_starting_face = -1;
			// Process a new Mesh with a unique material
			Mesh mesh;
			mesh.m_name = shape.name + "_" + materials[current_material_index].name;
			mesh.m_material_idx = current_material_index;
			mesh.m_start_index = vertices_so_far;
			number_of_materials_in_shape += 1;


			// IF WE DO THIS: We will need to have different starting/vertices_so_far for each posision, normal and UV texture coordinate
			// if we don't change all of them...
			// now we have collected all the indices for the vertices
			std::vector<int> m_indices(shape.mesh.indices.size());
			//std::unordered_set<std::array<int,3>> test;

			//std::unordered_set<int> test;
			for(const tinyobj::index_t& i: shape.mesh.indices ){
				m_indices.emplace_back(i.vertex_index); // sould we subtract one t
			
			}
			
			//std::unordered_set<int> uniqueIndeces(m_indices.begin(), m_indices.end());

			// now we need to collect the total vertices for that mesh...
			std::vector<glm::vec3> m_pos(attrib.vertices.size());
			for(const tinyobj::real_t& v: attrib.vertices){
				m_pos.emplace_back(glm::vec3(v));
			}
			// WE HAVE another problem, we now load all vectors and all indices for all meshes

			uint64_t number_of_faces = shape.mesh.indices.size() / 3; // takes all indices and divides them in number of faces(aka triangles)
			for(int i = current_material_starting_face; i < number_of_faces; i++)
			{
				if(shape.mesh.material_ids[i] != current_material_index)
				{
					if(next_material_index >= 0)
						continue;
					else if(finished_materials[shape.mesh.material_ids[i]])
						continue;
					else
					{ // Found a new material that we have not processed.
						next_material_index = shape.mesh.material_ids[i];
						next_material_starting_face = i;
					}
				}
				else
				{
					///////////////////////////////////////////////////////
					// Now we generate the vertices
					///////////////////////////////////////////////////////
					for(int j = 0; j < 3; j++)
					{
						int v = shape.mesh.indices[i * 3 + j].vertex_index;
						model->m_positions[vertices_so_far + j] =
						    glm::vec3(attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 0],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 1],
						              attrib.vertices[shape.mesh.indices[i * 3 + j].vertex_index * 3 + 2]);
						if(shape.mesh.indices[i * 3 + j].normal_index == -1)
						{
							// No normal, use the autogenerated
							model->m_normals[vertices_so_far + j] = glm::vec3(
							    auto_normals[shape.mesh.indices[i * 3 + j].vertex_index]);
						}
						else
						{
							model->m_normals[vertices_so_far + j] =
							    glm::vec3(attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 0],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 1],
							              attrib.normals[shape.mesh.indices[i * 3 + j].normal_index * 3 + 2]);
						}
						if(shape.mesh.indices[i * 3 + j].texcoord_index == -1)
						{
							// No UV coordinates. Use null.
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(0.0f);
						}
						else
						{
							model->m_texture_coordinates[vertices_so_far + j] = glm::vec2(
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 0],
							    attrib.texcoords[shape.mesh.indices[i * 3 + j].texcoord_index * 2 + 1]);
						}
					}
					vertices_so_far += 3;
				}
			}
			///////////////////////////////////////////////////////////////
			// Finalize and push this mesh to the list
			///////////////////////////////////////////////////////////////
			mesh.m_number_of_vertices = vertices_so_far - mesh.m_start_index;
			model->m_meshes.push_back(mesh);
			finished_materials[current_material_index] = true;
		}
		if(number_of_materials_in_shape == 1)
		{
			model->m_meshes.back().m_name = shape.name;
		}
	}

	///////////////////////////////////////////////////////////////////////
	// Upload to GPU
	///////////////////////////////////////////////////////////////////////
	glGenVertexArrays(1, &model->m_vaob);
	glBindVertexArray(model->m_vaob);
	glGenBuffers(1, &model->m_positions_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_positions_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_positions.size() * sizeof(glm::vec3), &model->m_positions[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(0);
	glGenBuffers(1, &model->m_normals_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_normals_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_normals.size() * sizeof(glm::vec3), &model->m_normals[0].x,
	             GL_STATIC_DRAW);
	glVertexAttribPointer(1, 3, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(1);
	glGenBuffers(1, &model->m_texture_coordinates_bo);
	glBindBuffer(GL_ARRAY_BUFFER, model->m_texture_coordinates_bo);
	glBufferData(GL_ARRAY_BUFFER, model->m_texture_coordinates.size() * sizeof(glm::vec2),
	             &model->m_texture_coordinates[0].x, GL_STATIC_DRAW);
	glVertexAttribPointer(2, 2, GL_FLOAT, false, 0, 0);
	glEnableVertexAttribArray(2);

	glBindVertexArray( 0 );
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	std::cout << "done.\n";
	return model;
}

void saveModelToOBJ(Model* model, std::string path)
{
	///////////////////////////////////////////////////////////////////////
	// Separate filename into directory, base filename and extension
	// NOTE: This can be made a LOT simpler as soon as compilers properly
	//		 support std::filesystem (C++17)
	///////////////////////////////////////////////////////////////////////
	size_t separator = path.find_last_of("\\/");
	std::string filename, extension, directory;
	if(separator != std::string::npos)
	{
		filename = path.substr(separator + 1, path.size() - separator - 1);
		directory = path.substr(0, separator + 1);
	}
	else
	{
		filename = path;
		directory = "./";
	}
	separator = filename.find_last_of(".");
	if(separator == std::string::npos)
	{
		std::cout << "Fatal: loadModelFromOBJ(): Expecting filename ending in '.obj'\n";
		exit(1);
	}
	extension = filename.substr(separator, filename.size() - separator);
	filename = filename.substr(0, separator);

	///////////////////////////////////////////////////////////////////////
	// Save Materials
	///////////////////////////////////////////////////////////////////////
	std::ofstream mat_file(directory + filename + ".mtl");
	if(!mat_file.is_open())
	{
		std::cout << "Could not open file " << filename << " for writing.\n";
		return;
	}
	mat_file << "# Exported by Chalmers Graphics Group\n";
	for(auto mat : model->m_materials)
	{
		mat_file << "newmtl " << mat.m_name << "\n";
		mat_file << "Kd " << mat.m_color.x << " " << mat.m_color.y << " " << mat.m_color.z << "\n";
		mat_file << "Ks " << mat.m_reflectivity << " " << mat.m_reflectivity << " " << mat.m_reflectivity
		         << "\n";
		mat_file << "Pm " << mat.m_metalness << "\n";
		mat_file << "Ps " << mat.m_fresnel << "\n";
		mat_file << "Pr " << mat.m_shininess << "\n";
		mat_file << "Ke " << mat.m_emission << " " << mat.m_emission << " " << mat.m_emission << "\n";
		mat_file << "Tf " << mat.m_transparency << " " << mat.m_transparency << " " << mat.m_transparency
		         << "\n";
		if(mat.m_color_texture.valid)
			mat_file << "map_Kd " << mat.m_color_texture.filename << "\n";
		if(mat.m_reflectivity_texture.valid)
			mat_file << "map_Ks " << mat.m_reflectivity_texture.filename << "\n";
		if(mat.m_metalness_texture.valid)
			mat_file << "map_Pm " << mat.m_metalness_texture.filename << "\n";
		if(mat.m_fresnel_texture.valid)
			mat_file << "map_Ps " << mat.m_fresnel_texture.filename << "\n";
		if(mat.m_shininess_texture.valid)
			mat_file << "map_Pr " << mat.m_shininess_texture.filename << "\n";
		if(mat.m_emission_texture.valid)
			mat_file << "map_Ke " << mat.m_emission_texture.filename << "\n";
	}
	mat_file.close();

	///////////////////////////////////////////////////////////////////////
	// Save geometry
	///////////////////////////////////////////////////////////////////////
	std::ofstream obj_file(directory + filename + ".obj");
	if(!obj_file.is_open())
	{
		std::cout << "Could not open file " << filename << " for writing.\n";
		return;
	}
	obj_file << "# Exported by Chalmers Graphics Group\n";
	obj_file << "mtllib " << filename << ".mtl\n";
	int vertex_counter = 1;
	for(auto mesh : model->m_meshes)
	{
		obj_file << "o " << mesh.m_name << "\n";
		obj_file << "g " << mesh.m_name << "\n";
		obj_file << "usemtl " << model->m_materials[mesh.m_material_idx].m_name << "\n";
		for(uint32_t i = mesh.m_start_index; i < mesh.m_start_index + mesh.m_number_of_vertices; i++)
		{
			obj_file << "v " << model->m_positions[i].x << " " << model->m_positions[i].y << " "
			         << model->m_positions[i].z << "\n";
		}
		for(uint32_t i = mesh.m_start_index; i < mesh.m_start_index + mesh.m_number_of_vertices; i++)
		{
			obj_file << "vn " << model->m_normals[i].x << " " << model->m_normals[i].y << " "
			         << model->m_normals[i].z << "\n";
		}
		for(uint32_t i = mesh.m_start_index; i < mesh.m_start_index + mesh.m_number_of_vertices; i++)
		{
			obj_file << "vt " << model->m_texture_coordinates[i].x << " " << model->m_texture_coordinates[i].y
			         << "\n";
		}
		int number_of_faces = mesh.m_number_of_vertices / 3;
		for(int i = 0; i < number_of_faces; i++)
		{
			obj_file << "f " << vertex_counter << "/" << vertex_counter << "/" << vertex_counter << " "
			         << vertex_counter + 1 << "/" << vertex_counter + 1 << "/" << vertex_counter + 1 << " "
			         << vertex_counter + 2 << "/" << vertex_counter + 2 << "/" << vertex_counter + 2 << "\n";
			vertex_counter += 3;
		}
	}
}

///////////////////////////////////////////////////////////////////////
// Free model
///////////////////////////////////////////////////////////////////////
void freeModel(Model* model)
{
	if(model != nullptr)
		delete model;
}

///////////////////////////////////////////////////////////////////////
// Loop through all Meshes in the Model and render them
///////////////////////////////////////////////////////////////////////
void render(const Model* model, const bool submitMaterials)
{
	glBindVertexArray(model->m_vaob);
	for(auto& mesh : model->m_meshes)
	{
		if(submitMaterials)
		{
			const Material& material = model->m_materials[mesh.m_material_idx];

			bool has_color_texture = material.m_color_texture.valid;
			bool has_reflectivity_texture = material.m_reflectivity_texture.valid;
			bool has_metalness_texture = material.m_metalness_texture.valid;
			bool has_fresnel_texture = material.m_fresnel_texture.valid;
			bool has_shininess_texture = material.m_shininess_texture.valid;
			bool has_emission_texture = material.m_emission_texture.valid;
			if ( has_color_texture )
			{
				glActiveTexture( GL_TEXTURE0 );
				glBindTexture( GL_TEXTURE_2D, material.m_color_texture.gl_id );
			}
			if(has_reflectivity_texture)
			{
				glActiveTexture( GL_TEXTURE1 );
				glBindTexture( GL_TEXTURE_2D, material.m_reflectivity_texture.gl_id );
			}
			if ( has_metalness_texture )
			{
				glActiveTexture( GL_TEXTURE2 );
				glBindTexture( GL_TEXTURE_2D, material.m_metalness_texture.gl_id );
			}
			if ( has_fresnel_texture )
			{
				glActiveTexture( GL_TEXTURE3 );
				glBindTexture( GL_TEXTURE_2D, material.m_fresnel_texture.gl_id );
			}
			if ( has_shininess_texture )
			{
				glActiveTexture( GL_TEXTURE4 );
				glBindTexture( GL_TEXTURE_2D, material.m_shininess_texture.gl_id );
			}
			if ( has_emission_texture )
			{
				glActiveTexture( GL_TEXTURE5 );
				glBindTexture( GL_TEXTURE_2D, material.m_emission_texture.gl_id );
			}
			glActiveTexture( GL_TEXTURE0 );
			GLint current_program = 0;
			glGetIntegerv(GL_CURRENT_PROGRAM, &current_program);

			setUniformSlow( current_program, "has_color_texture", has_color_texture );
			setUniformSlow( current_program, "has_emission_texture", has_emission_texture );

			setUniformSlow( current_program, "material_color", material.m_color );
			setUniformSlow( current_program, "material_reflectivity", material.m_reflectivity );
			setUniformSlow( current_program, "material_metalness", material.m_metalness );
			setUniformSlow( current_program, "material_fresnel", material.m_fresnel );
			setUniformSlow( current_program, "material_shininess", material.m_shininess );
			setUniformSlow( current_program, "material_emission", material.m_emission );

			// Actually unused in the labs
			setUniformSlow( current_program, "has_reflectivity_texture", has_reflectivity_texture );
			setUniformSlow( current_program, "has_metalness_texture", has_metalness_texture );
			setUniformSlow( current_program, "has_fresnel_texture", has_fresnel_texture );
			setUniformSlow( current_program, "has_shininess_texture", has_shininess_texture );

		}
		glDrawArrays(GL_TRIANGLES, mesh.m_start_index, (GLsizei)mesh.m_number_of_vertices);
	}
	glBindVertexArray(0);
}
} // namespace labhelper
