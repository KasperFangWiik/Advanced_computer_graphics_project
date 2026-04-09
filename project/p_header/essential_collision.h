#pragma once
//#include<iostream>
#include<limits> // behövs????
#include <vector>
#include <array>
#include <glm/glm.hpp>
using namespace glm;

//#include "Standard_Vectorfunc.h"
//#include<concepts>
//#include"Entity_remake.h" // concept IsCollideble is the only reason to include this, and i only use it in chunk for know... 

namespace col {
struct CollisionResponseData {
    float penetration;
    glm::vec3 contact_normal;

    glm::vec3 respons() const{ // const?
        return penetration * contact_normal;
    }

};

// maby include the center of the shape......
struct ConvexCollider{
    std::vector<glm::vec3> vertices; // should be a pointer???
    unsigned int id;
    // instead of id we could just include a pointer to the matrix associated, but also probably need id to know if entity been deleted...

    ConvexCollider(){}
    ConvexCollider(std::vector<glm::vec3> verts, unsigned int i)
    : vertices(std::move(verts)), id(i) {}
};

/*
struct SweptCircleShape {
    sf::CircleShape& circle;
    sf::Vector2f dirV;
    float speed;
};

template<typename U>
//concept IsCollideble = std::is_same_v< U, Id_Pair<sf::RectangleShape> > || std::is_same_v< U, Id_Pair<sf::CircleShape>> || std::is_same_v< U, Id_Pair<sf::ConvexShape> >;
concept IsCollideble = std::is_same_v< U,sf::RectangleShape> || std::is_same_v< U, sf::CircleShape> || std::is_same_v< U, sf::ConvexShape >;
*/


/*
------------------------------------------------------------------------------------------------------------------
Pure SAT related:
------------------------------------------------------------------------------------------------------------------
*/

bool check_SAT_axis_overlap(const std::array<float, 2>& min_max_dist1,
                            const std::array<float, 2>& min_max_dist2);

bool check_SAT_axis_overlap(const glm::vec3& projection_axis,
                            const std::array<float, 2>& min_max_dist1,
                            const std::array<float, 2>& min_max_dist2,
                            CollisionResponseData& respons_data);

/*
------------------------------------------------------------------------------------------------------------------
ConvexShape collisions related:
------------------------------------------------------------------------------------------------------------------
*/
//std::vector<glm::vec3> get_vertecis_of_ConvexShape(sf::ConvexShape& colid_sprite);

glm::vec3 calc_normal_of_lineSegment(const glm::vec3& starting_point, const glm::vec3& end_point);

std::vector<glm::vec3> normals_of_ConvexShape(const std::vector<glm::vec3>& poly_vertices);

const std::array<float, 2> min_max_projection_distance(
                                                const glm::vec3& projection_axis,
                                                const std::vector<glm::vec3>& vertices);

bool collision(ConvexCollider& poly1, ConvexCollider& poly2, glm::vec3& respons_vector);

/*
------------------------------------------------------------------------------------------------------------------
Circle v ConvexShape collisions related:
------------------------------------------------------------------------------------------------------------------
*/
/*
sf::Vector2f closest_polyVertex_to_point(sf::Vector2f& point, std::vector<glm::vec3>& rect2_vertices);

bool collision(sf::CircleShape& circle1, sf::ConvexShape& poly2, sf::Vector2f& respons_vector);

bool collision(sf::ConvexShape& poly1, sf::CircleShape& circle2, sf::Vector2f& respons_vector);
*/
        

/*
------------------------------------------------------------------------------------------------------------------
Circle collisions related:
------------------------------------------------------------------------------------------------------------------
*/
/*
bool intersect(sf::CircleShape& circle1, sf::CircleShape& circle2);
bool collision(sf::CircleShape& circle1, sf::CircleShape& circle2, sf::Vector2f& respons_vector);
*/

}