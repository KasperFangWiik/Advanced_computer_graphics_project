#include "../p_header/essential_collision.h"

namespace col {
/*
------------------------------------------------------------------------------------------------------------------
Pure SAT related:
------------------------------------------------------------------------------------------------------------------
*/

bool check_SAT_axis_overlap(const std::array<float, 2>& min_max_dist1,
                            const std::array<float, 2>& min_max_dist2) {

    const float min_vertex1 = min_max_dist1.at(0);
    const float max_vertex1 = min_max_dist1.at(1);

    const float min_vertex2 = min_max_dist2.at(0);
    const float max_vertex2 = min_max_dist2.at(1);

    return (min_vertex1 <= min_vertex2 && max_vertex1 >= min_vertex2) ||
           (min_vertex2 <= min_vertex1 && max_vertex2 >= min_vertex1);
}


bool check_SAT_axis_overlap(
    const glm::vec3& projection_axis,
    const std::array<float, 2>& min_max_dist1,
    const std::array<float, 2>& min_max_dist2,
    CollisionResponseData& respons_data
) {

    const float min_vertex1 = min_max_dist1.at(0);
    const float max_vertex1 = min_max_dist1.at(1);

    const float min_vertex2 = min_max_dist2.at(0);
    const float max_vertex2 = min_max_dist2.at(1);

    //float current_size_of_overlap = 0.0f;

    // (Overlap in order object 1 -> object 2)
    const bool obj1_X_obj2 = min_vertex1 <= min_vertex2 && max_vertex1 >= min_vertex2;

    // (Overlap in order object 2 -> object 1)
    const bool obj2_X_obj1 = min_vertex2 <= min_vertex1 && max_vertex2 >= min_vertex1;

    if (obj1_X_obj2 || obj2_X_obj1) {

        float current_size_of_overlap = obj2_X_obj1 ? (min_vertex1 - max_vertex2) : (min_vertex2 - max_vertex1);
        if (std::abs(current_size_of_overlap) < std::abs(respons_data.penetration)) { // abs from cstdlib, should use abs or multiply?
            respons_data.penetration = current_size_of_overlap;
            respons_data.contact_normal = obj2_X_obj1 ? -projection_axis : projection_axis;
        }

        return true;
    }

    return false;
}

/*
------------------------------------------------------------------------------------------------------------------
ConvexShape collisions:
------------------------------------------------------------------------------------------------------------------
*/

/*
std::vector<glm::vec3> get_vertecis_of_ConvexShape(sf::ConvexShape& colid_sprite) {

    std::vector< sf::Vector2f> vertices = {};
    sf::Transform transformMatrix = colid_sprite.getTransform();

    const size_t numb_of_vertices = colid_sprite.getPointCount();
    vertices.reserve(numb_of_vertices);

    for (size_t n{}; n < numb_of_vertices; n++) {
        vertices.emplace_back(transformMatrix * colid_sprite.getPoint(n));
    }
    return vertices;
}
*/


glm::vec3 calc_normal_of_lineSegment(const glm::vec3& starting_point, const glm::vec3& end_point) {

    glm::vec3 edge = end_point - starting_point;
    glm::vec3 normal = glm::vec3( edge.z, 0.0f, -edge.x) ; //(x,y,z)
    

    if (normal != glm::vec3(0.0f)) 
        normal = glm::normalize(normal);

        //normal *= (1.0f / glm::length(normal));
    // glm::normalize(normal); this returns a new vector and ode nor effect the normal inputted..
    return normal;
}

std::vector<glm::vec3> normals_of_ConvexShape(const std::vector<glm::vec3>& poly_vertices) {

    // Function could be generalized to all 
    std::vector<glm::vec3> normals = {};
    const size_t numb_of_vertices = poly_vertices.size();
    normals.reserve(numb_of_vertices);

    for (size_t n{}; n < numb_of_vertices; n++) {
        size_t n_vrap_around = (n+1) % numb_of_vertices;
        normals.emplace_back(calc_normal_of_lineSegment(poly_vertices[n], poly_vertices[n_vrap_around]));
    }

    return normals;
}

const std::array<float, 2> min_max_circle_projection_distance(
                            const glm::vec3& projection_axis,
                            const std::vector<glm::vec3>& vertices){

    const glm::vec3& circleCenter = vertices[0];
    const float& radius = vertices[1].r;

    const float& circle_pos_on_axis = glm::dot(circleCenter, projection_axis);

    float min_dist = circle_pos_on_axis - radius;
    float max_dist = circle_pos_on_axis + radius;
    /*
    // Chat says i can remove this...
    if (min_dist > max_dist) {
        std::swap(min_dist, max_dist);
    }
    */
    return { min_dist, max_dist };
}

const std::array<float, 2> min_max_projection_distance(
                            const glm::vec3& projection_axis,
                            const std::vector<glm::vec3>& vertices
) {
    // if we are calculating the projection distance of a circle
    if(vertices.size() == 2)
     return min_max_circle_projection_distance(projection_axis, vertices);

    float min_distance = dot(vertices.at(0), projection_axis);
    float max_distance = dot(vertices.at(1), projection_axis);

    if (min_distance > max_distance) {
        std::swap(min_distance, max_distance);
    }

    float tmp_distance = {};
    for (int i = 2; i < vertices.size(); i++) {
        tmp_distance = dot(vertices.at(i), projection_axis);

        if (min_distance > tmp_distance) {
            min_distance = tmp_distance;
        }

        if (max_distance < tmp_distance) {
            max_distance = tmp_distance;
        }
    }

    return { min_distance, max_distance };
}

glm::vec3 closest_polyVertex_to_point(const glm::vec3& point, const std::vector<glm::vec3>& rect2_vertices) {

    glm::vec3 DirVecToClosestVertex{};
    float minnfloatVal = std::numeric_limits<float>::max();
    for (const glm::vec3& n : rect2_vertices) {
        glm::vec3 tmp_v = n - point;
        float squaredDistToCenter = glm::dot(tmp_v, tmp_v);
        if (minnfloatVal > squaredDistToCenter) {
            minnfloatVal = squaredDistToCenter;
            DirVecToClosestVertex = tmp_v;
        }
    }
    return DirVecToClosestVertex;
}

const glm::vec3 get_circle_normal(const vec3& circleCenter, const std::vector<glm::vec3>& PoligonVertices){

    const glm::vec3& DirVecToClosestVertex = closest_polyVertex_to_point(circleCenter, PoligonVertices);
    //const glm::vec3& normal = DirVecToClosestVertex;
    return  (DirVecToClosestVertex != glm::vec3(0.0f))? glm::normalize(DirVecToClosestVertex) : DirVecToClosestVertex;
}

std::vector<glm::vec3> get_SAT_normals(std::vector<glm::vec3>& poly1, std::vector<glm::vec3>& poly2) {

    size_t numbVert1 = poly1.size();
    size_t numbVert2 = poly2.size();

    std::vector<glm::vec3>  normals;
    normals.reserve(numbVert1 + numbVert2); // the number of normals are equal to number of vertices when over 3 vertices exists right?

    if(numbVert1 == 2)
        normals.emplace_back(get_circle_normal(poly1[0], poly2));
    else {
        const std::vector<glm::vec3>& n1 = normals_of_ConvexShape(poly1);
        normals.insert(normals.end(), n1.begin(), n1.end());
    }

    if(numbVert2 == 2)
        normals.emplace_back(get_circle_normal(poly2[0], poly1));
    else {
        const std::vector<glm::vec3>& n2 = normals_of_ConvexShape(poly2);
        normals.insert(normals.end(), n2.begin(), n2.end());
    }

    return normals; 
}

bool circle_collision(std::vector<glm::vec3>& circle1, std::vector<glm::vec3>& circle2, glm::vec3& respons_vector) {

    CollisionResponseData respons_data = {};

    const glm::vec3& center_cercle1 = circle1[0];
    const glm::vec3& center_cercle2 = circle2[0];

    // maby we only need one center and another radius value for the circle, like only x has the radius value...(x,0.0f,0.0f) 
    const float& radius1 = circle1[1].r;//glm::distance(circle1[0], circle1[2]); // circle1.vertices[2].r;// rgb so r should contain same as x...
    const float& radius2 = circle2[1].r;//glm::distance(circle2[0], circle2[2]); // circle2.vertices[2].r;

    glm::vec3 Dir_vector_from_1to2 = (center_cercle1 - center_cercle2);
    const float& distance_between = glm::length(Dir_vector_from_1to2); //glm::distance(center_cercle1, center_cercle2);

    if (Dir_vector_from_1to2 != glm::vec3(0.0f))
        Dir_vector_from_1to2 = glm::normalize(Dir_vector_from_1to2);

    const float penetrationGap = distance_between - (radius1 + radius2);
    if (penetrationGap < 0) {
        respons_data.penetration = penetrationGap;
        respons_data.contact_normal = Dir_vector_from_1to2;
        respons_vector = respons_data.penetration * respons_data.contact_normal;
        return true;
    }
    return false;
}

bool collision(std::vector<glm::vec3>& poly1, std::vector<glm::vec3>& poly2, glm::vec3& respons_vector) {

    // this should work for circle v circle intersections(might lead to tunneling because projectiles move faster.....) 
    if (poly1.size() == 2 && poly2.size() == 2)
        return circle_collision(poly1, poly2, respons_vector);

    // future maby make one function that returns all normals, easier to change between circle and convex polygon collision.
    // but how should i switch to easier collision/intersection styles
   // SHOULD I USE std::move
   /*
    std::vector<glm::vec3>  normals(std::move(normals_of_ConvexShape(poly1.vertices)));
    std::vector<glm::vec3>  n2(std::move(normals_of_ConvexShape(poly2.vertices)));
    
    normals.reserve(normals.size() + n2.size());
    normals.insert(normals.end(), n2.begin(), n2.end());
    */

    std::vector<glm::vec3> normals(get_SAT_normals(poly1, poly2));

    CollisionResponseData respons_data = { std::numeric_limits<float>::max(), {} };
    for (const glm::vec3& n : normals) {
        const std::array<float, 2> min_max_dist1 = min_max_projection_distance(n, poly1);
        const std::array<float, 2> min_max_dist2 = min_max_projection_distance(n, poly2);

        if (!check_SAT_axis_overlap(n, min_max_dist1, min_max_dist2, respons_data))
            return false;
    }

    respons_vector = respons_data.penetration * respons_data.contact_normal;
    return true;
}


}