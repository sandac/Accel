#include "KDTreeCPU.h"
#include <algorithm>
#include "Intersections.h"
#include <iostream>


////////////////////////////////////////////////////
// Constructor/destructor.
////////////////////////////////////////////////////

KDTreeCPU::KDTreeCPU( int num_tris, glm::vec3 *tris, int num_verts, glm::vec3 *verts )
{
	// Set class-level variables.
	num_levels = 0;
	num_leaves = 0;
	this->num_verts = num_verts;
	this->verts = verts;
	this->num_tris = num_tris;
	this->tris = tris;

	// Create list of triangle indices for first level of kd-tree.
	int *tri_indices = new int[num_tris];
	for ( int i = 0; i < num_tris; ++i ) {
		tri_indices[i] = i;
	}

	// Compute bounding box for all triangles.
	boundingBox bbox = computeTightFittingBoundingBox( num_verts, verts );

	// Build kd-tree and set root node.
	root = constructTreeMedianSpaceSplit( num_tris, tri_indices, bbox, 1 );
}

KDTreeCPU::~KDTreeCPU()
{
	if ( num_verts > 0 ) {
		delete[] verts;
	}

	if ( num_tris > 0 ) {
		delete[] tris;
	}

	delete root;
}


////////////////////////////////////////////////////
// Getters.
////////////////////////////////////////////////////

KDTreeNode* KDTreeCPU::getRootNode() const
{
	return root;
}

int KDTreeCPU::getNumLevels() const
{
	return num_levels;
}

int KDTreeCPU::getNumLeaves( void ) const
{
	return num_leaves;
}

int KDTreeCPU::getLongestBoundingBoxSide( glm::vec3 min, glm::vec3 max )
{
	// max > min is guaranteed.
	float xlength = max.x - min.x;
	float ylength = max.y - min.y;
	float zlength = max.z - min.z;
	return ( xlength > ylength && xlength > zlength ) ? X_AXIS : ( ylength > zlength ? Y_AXIS : Z_AXIS );
}

float KDTreeCPU::getMinTriValue( int tri_index, SplitAxis axis )
{
	glm::vec3 tri = tris[tri_index];
	glm::vec3 v0 = verts[( int )tri[0]];
	glm::vec3 v1 = verts[( int )tri[1]];
	glm::vec3 v2 = verts[( int )tri[2]];

	if ( axis == X_AXIS ) {
		return ( v0.x < v1.x && v0.x < v2.x ) ? v0.x : ( v1.x < v2.x ? v1.x : v2.x );
	}
	else if ( axis == Y_AXIS ) {
		return ( v0.y < v1.y && v0.y < v2.y ) ? v0.y : ( v1.y < v2.y ? v1.y : v2.y );
	}
	else {
		return ( v0.z < v1.z && v0.z < v2.z ) ? v0.z : ( v1.z < v2.z ? v1.z : v2.z );
	}
}

float KDTreeCPU::getMaxTriValue( int tri_index, SplitAxis axis )
{
	glm::vec3 tri = tris[tri_index];
	glm::vec3 v0 = verts[( int )tri[0]];
	glm::vec3 v1 = verts[( int )tri[1]];
	glm::vec3 v2 = verts[( int )tri[2]];

	if ( axis == X_AXIS ) {
		return ( v0.x > v1.x && v0.x > v2.x ) ? v0.x : ( v1.x > v2.x ? v1.x : v2.x );
	}
	else if ( axis == Y_AXIS ) {
		return ( v0.y > v1.y && v0.y > v2.y ) ? v0.y : ( v1.y > v2.y ? v1.y : v2.y );
	}
	else {
		return ( v0.z > v1.z && v0.z > v2.z ) ? v0.z : ( v1.z > v2.z ? v1.z : v2.z );
	}
}


////////////////////////////////////////////////////
// Compute tight fitting bounding box around triangles given a list of vertices.
////////////////////////////////////////////////////
boundingBox KDTreeCPU::computeTightFittingBoundingBox( int num_verts, glm::vec3 *verts )
{
	// Compute bounding box for input mesh.
	glm::vec3 max = glm::vec3( -INFINITY, -INFINITY, -INFINITY );
	glm::vec3 min = glm::vec3( INFINITY, INFINITY, INFINITY );

	for ( int i = 0; i < num_verts; ++i ) {
		if ( verts[i].x < min.x ) {
			min.x = verts[i].x;
		}
		if ( verts[i].y < min.y ) {
			min.y = verts[i].y;
		}
		if ( verts[i].z < min.z ) {
			min.z = verts[i].z;
		}
		if ( verts[i].x > max.x ) {
			max.x = verts[i].x;
		}
		if ( verts[i].y > max.y ) {
			max.y = verts[i].y;
		}
		if ( verts[i].z > max.z ) {
			max.z = verts[i].z;
		}
	}

	boundingBox bbox;
	bbox.min = min;
	bbox.max = max;

	return bbox;
}


////////////////////////////////////////////////////
// Compute tight fitting bounding box around triangles given a list of triangle indices.
////////////////////////////////////////////////////
boundingBox KDTreeCPU::computeTightFittingBoundingBox( int num_tris, int *tri_indices )
{
	int num_verts = num_tris * 3;
	glm::vec3 *verts = new glm::vec3[num_verts];

	int verts_index;
	for ( int i = 0; i < num_tris; ++i ) {
		glm::vec3 tri = tris[i];
		verts_index = i * 3;
		verts[verts_index + 0] = verts[( int )tri[0]];
		verts[verts_index + 1] = verts[( int )tri[1]];
		verts[verts_index + 2] = verts[( int )tri[2]];
	}

	boundingBox bbox = computeTightFittingBoundingBox( num_verts, verts );
	delete[] verts;
	return bbox;
}


////////////////////////////////////////////////////
// constructTreeMedianSpaceSplit().
////////////////////////////////////////////////////
KDTreeNode* KDTreeCPU::constructTreeMedianSpaceSplit( int num_tris, int *tri_indices, boundingBox bounds, int curr_depth )
{
	// Create new node.
	KDTreeNode *node = new KDTreeNode();
	node->num_tris = num_tris;
	node->tri_indices = tri_indices;

	// Override passed-in bounding box and create "tightest-fitting" bounding box around passed-in list of triangles.
	if ( USE_TIGHT_FITTING_BOUNDING_BOXES ) {
		node->bbox = computeTightFittingBoundingBox( num_tris, tri_indices );
	}
	else {
		node->bbox = bounds;
	}

	// Base case--Number of triangles in node is small enough.
	if ( num_tris <= NUM_TRIS_PER_NODE ) {
		// Update number of tree levels.
		if ( curr_depth > num_levels ) {
			num_levels = curr_depth;
		}

		// Return leaf node.
		++num_leaves;
		return node;
	}

	// Get longest side of bounding box.
	int longest_side = getLongestBoundingBoxSide( bounds.min, bounds.max );

	// Compute median value for longest side as well as "loose-fitting" bounding boxes.
	float median_val = 0.0;
	boundingBox left_bbox = bounds;
	boundingBox right_bbox = bounds;
	if ( longest_side == X_AXIS ) {
		median_val = bounds.min.x + ( ( bounds.max.x - bounds.min.x ) / 2.0f );
		left_bbox.max.x = median_val;
		right_bbox.min.x = median_val;
	}
	else if ( longest_side == Y_AXIS ) {
		median_val = bounds.min.y + ( ( bounds.max.y - bounds.min.y ) / 2.0f );
		left_bbox.max.y = median_val;
		right_bbox.min.y = median_val;
	}
	else {
		median_val = bounds.min.z + ( ( bounds.max.z - bounds.min.z ) / 2.0f );
		left_bbox.max.z = median_val;
		right_bbox.min.z = median_val;
	}

	// Allocate and initialize memory for temporary buffers to hold triangle indices for left and right subtrees.
	int *temp_left_tri_indices = new int[num_tris];
	int *temp_right_tri_indices = new int[num_tris];

	// Populate temporary buffers.
	int left_tri_count = 0, right_tri_count = 0;
	float min_tri_val, max_tri_val;
	for ( int i = 0; i < num_tris; ++i ) {
		// Get min and max triangle values along desired axis.
		if ( longest_side == X_AXIS ) {
			min_tri_val = getMinTriValue( tri_indices[i], X_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], X_AXIS );
		}
		else if ( longest_side == Y_AXIS ) {
			min_tri_val = getMinTriValue( tri_indices[i], Y_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], Y_AXIS );
		}
		else {
			min_tri_val = getMinTriValue( tri_indices[i], Z_AXIS );
			max_tri_val = getMaxTriValue( tri_indices[i], Z_AXIS );
		}

		// Update temp_left_tri_indices.
		if ( min_tri_val < median_val ) {
			temp_left_tri_indices[i] = tri_indices[i];
			++left_tri_count;
		}
		else {
			temp_left_tri_indices[i] = -1;
		}

		// Update temp_right_tri_indices.
		if ( max_tri_val >= median_val ) {
			temp_right_tri_indices[i] = tri_indices[i];
			++right_tri_count;
		}
		else {
			temp_right_tri_indices[i] = -1;
		}
	}

	// Allocate memory for lists of triangle indices for left and right subtrees.
	int *left_tri_indices = new int[left_tri_count];
	int *right_tri_indices = new int[right_tri_count];

	// Populate lists of triangle indices.
	int left_index = 0, right_index = 0;
	for ( int i = 0; i < num_tris; ++i ) {
		if ( temp_left_tri_indices[i] != -1 ) {
			left_tri_indices[left_index] = temp_left_tri_indices[i];
			++left_index;
		}
		if ( temp_right_tri_indices[i] != -1 ) {
			right_tri_indices[right_index] = temp_right_tri_indices[i];
			++right_index;
		}
	}

	// Free temporary triangle indices buffers.
	delete[] temp_left_tri_indices;
	delete[] temp_right_tri_indices;

	// Recurse.
	node->left = constructTreeMedianSpaceSplit( left_tri_count, left_tri_indices, left_bbox, curr_depth + 1 );
	node->right = constructTreeMedianSpaceSplit( right_tri_count, right_tri_indices, right_bbox, curr_depth + 1 );

	return node;
}


////////////////////////////////////////////////////
// Public kd-tree traversal method to test for intersections with passed-in ray.
////////////////////////////////////////////////////
bool KDTreeCPU::intersect( const glm::vec3 &ray_o, const glm::vec3 &ray_dir, float &t, glm::vec3 &hit_point, glm::vec3 &normal ) const
{
	t = INFINITY;
	bool hit = intersect( root, ray_o, ray_dir, t, normal );
	if ( hit ) {
		hit_point = ray_o + ( t * ray_dir );
	}
	return hit;
}


////////////////////////////////////////////////////
// Recursive kd-tree traversal method to test for intersections with passed-in ray.
////////////////////////////////////////////////////
bool KDTreeCPU::intersect( KDTreeNode *curr_node, const glm::vec3 &ray_o, const glm::vec3 &ray_dir, float &t, glm::vec3 &normal ) const
{
	// Perform ray/AABB intersection test.
	float t_near, t_far;
	bool intersects_aabb = Intersections::aabbIntersect( curr_node->bbox, ray_o, ray_dir, t_near, t_far );

	if ( intersects_aabb ) {
		// If current node is a leaf node.
		if ( !curr_node->left && !curr_node->right ) {
			// Check triangles for intersections.
			bool intersection_detected = false;
			for ( int i = 0; i < curr_node->num_tris; ++i ) {
				glm::vec3 tri = tris[curr_node->tri_indices[i]];
				glm::vec3 v0 = verts[( int )tri[0]];
				glm::vec3 v1 = verts[( int )tri[1]];
				glm::vec3 v2 = verts[( int )tri[2]];

				// Perform ray/triangle intersection test.
				float tmp_t = INFINITY;
				glm::vec3 tmp_normal( 0.0f, 0.0f, 0.0f );
				bool intersects_tri = Intersections::triIntersect( ray_o, ray_dir, v0, v1, v2, tmp_t, tmp_normal );

				if ( intersects_tri ) {
					intersection_detected = true;
					if ( tmp_t < t ) {
						t = tmp_t;
						normal = tmp_normal;
					}
				}
			}

			return intersection_detected;
		}
		// Else, recurse.
		else {
			bool hit_left = false, hit_right = false;
			if ( curr_node->left ) {
				hit_left = intersect( curr_node->left, ray_o, ray_dir, t, normal );
			}
			if ( curr_node->right ) {
				hit_right = intersect( curr_node->right, ray_o, ray_dir, t, normal );
			}
			return hit_left || hit_right;
		}
	}

	return false;
}


////////////////////////////////////////////////////
// Debug methods.
////////////////////////////////////////////////////

void KDTreeCPU::printNumTrianglesInEachNode( KDTreeNode *curr_node, int curr_depth )
{
	std::cout << "Level: " << curr_depth << ", Triangles: " << curr_node->num_tris << std::endl;

	if ( curr_node->left ) {
		printNumTrianglesInEachNode( curr_node->left, curr_depth + 1 );
	}
	if ( curr_node->right ) {
		printNumTrianglesInEachNode( curr_node->right, curr_depth + 1 );
	}
}