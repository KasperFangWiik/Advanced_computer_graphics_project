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
ConvexShape collisions related:
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


glm::vec3 calc_normal_of_lineSegment(const glm::vec3& starting_point, const glm::vec3& end_point){

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

const std::array<float, 2> min_max_projection_distance(
                            const glm::vec3& projection_axis,
                            const std::vector<glm::vec3>& vertices
){
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

bool collision(ConvexCollider& poly1, ConvexCollider& poly2, glm::vec3& respons_vector) {

    // the move might be invalidating the elements in std::vector...
    //std::vector<glm::vec3> vertecis_poly1(std::move(poly1.vertices));
    std::vector<glm::vec3>  normals_1(normals_of_ConvexShape(poly1.vertices));

    //std::vector<glm::vec3> vertecis_poly2(std::move(poly2.vertices));
    std::vector<glm::vec3>  normals_2(normals_of_ConvexShape(poly2.vertices));

    CollisionResponseData respons_data = { std::numeric_limits<float>::max(), {} };

    for (const glm::vec3& n1 : normals_1) {
        const std::array<float, 2> min_max_dist1 = min_max_projection_distance(n1, poly1.vertices);
        const std::array<float, 2> min_max_dist2 = min_max_projection_distance(n1, poly2.vertices);
        
        if (!check_SAT_axis_overlap(n1, min_max_dist1, min_max_dist2, respons_data))
            return false;
        }


    for (const glm::vec3& n2 : normals_2) {

        const std::array<float, 2> min_max_dist1 = min_max_projection_distance(n2, poly1.vertices);
        const std::array<float, 2> min_max_dist2 = min_max_projection_distance(n2, poly2.vertices);

        if (!check_SAT_axis_overlap(n2, min_max_dist1, min_max_dist2, respons_data))
            return false;
    }

    respons_vector = respons_data.penetration * respons_data.contact_normal;
    return true;
}



/*
------------------------------------------------------------------------------------------------------------------
Circle v ConvexShape collisions related:
------------------------------------------------------------------------------------------------------------------
*/

/*
// kan generalisera denna... Med std::vector
sf::Vector2f closest_polyVertex_to_point(sf::Vector2f& point, std::vector<sf::Vector2f>& rect2_vertices) {

    sf::Vector2f closest_vertex{};
    float minnfloatVal = std::numeric_limits<float>::max();
    for (const sf::Vector2f& n : rect2_vertices) {
        float dist_to_center = squared_distance_between_points(n, point);
        if (minnfloatVal > dist_to_center) {
            minnfloatVal = dist_to_center;
            closest_vertex = n;
        }
    }
    return closest_vertex;
}

bool collision(sf::CircleShape& circle1, sf::ConvexShape& poly2, sf::Vector2f& respons_vector) {

    float radius = circle1.getRadius();
    sf::Vector2f circle1_ceter = circle1.getTransform() * (circle1.getOrigin() + sf::Vector2f{ radius, radius });
    //sf::Vector2f circle1_ceter = circle1.getTransform() * circle1.getGeometricCenter();//circle1.getOrigin();

    std::vector<sf::Vector2f> vertecis_poly2(std::move(get_vertecis_of_ConvexShape(poly2)));
    std::vector<sf::Vector2f> normals_2(std::move(normals_of_ConvexShape(vertecis_poly2)));

    sf::Vector2f closest_vertex = closest_polyVertex_to_point(circle1_ceter, vertecis_poly2);

    sf::Vector2f circle_normal{};
    sf::Vector2f distance_difference_vector = (closest_vertex - circle1_ceter);

    circle_normal = (distance_difference_vector != sf::Vector2f{ 0.0, 0.0 })? distance_difference_vector.normalized() : distance_difference_vector;

    std::vector<sf::Vector2f> normals(std::move(normals_2));
    normals.emplace_back(circle_normal);

    CollisionResponseData respons_data = { std::numeric_limits<float>::max(), {} };

    for (const sf::Vector2f& normal : normals) {

        const std::array<float, 2> min_max_dist1 = min_max_projection_distance(normal, circle1_ceter, radius);
        const std::array<float, 2> min_max_dist2 = min_max_projection_distance(normal, vertecis_poly2);

        if (!check_SAT_axis_overlap(normal, min_max_dist1, min_max_dist2, respons_data))
            return false;
    }
    respons_vector = respons_data.penetration * respons_data.contact_normal;

    return true;
}


//bool intersect(sf::ConvexShape& rect1, sf::CircleShape& circle2) {
//    return intersect(circle2, rect1);
//}
bool collision(sf::ConvexShape& poly1, sf::CircleShape& circle2, sf::Vector2f& respons_vector) {

    bool collide = collision(circle2, poly1, respons_vector);
    respons_vector *= -1.f;
    return collide;
}
*/
}