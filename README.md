# QuickObjLoader
This is a relativly quick, header-only .obj loader using C++20 and GLM.

Only testing it with MSVC at the moment, but it should work with other compilers too.

**Free to raise PRs and issues and feature requests.** 

## How it works
Groups the .obj's file content into meshes and also loads the material information.

All meshes has its own Verticies and Indicies list that you can simply pass into a vertex and index buffer(using it with OpenGL).

The meshes are built-up from groups that contain the range of indicies and its **later** materials. So using the groups you can even render the group on its own.

## Supports
Supports all vertex, face types, normals, texture coords.
For faces that has more than 3 verticies, it generates the proper fan triangle set.
Loads the standard material attributes.

## Does not support perfectly
Storing material for the indicies/faces.

## A quick documentation:
You can load the obj file using the constructor by passing the filepath or using the bool 

 - bool **LoadFile**(std::string_view) : Loads the given .obj file, returns false if cant open the file
 - std::vector<Mesh>& **GetMeshes**() :  Returns all the parsed meshes

### Mesh
#### The main, undepenent parts of the 

 - std::string **Name**: Name of the mesh
 - std::vector<Vertex> **Vertices**: Contains all the verticies in the mesh
 - std::vector<uint32_t> **Indices**: All the indicies in the mesh, can be put directly copied into the indexbuffer
 - std::vector<Material> **Materials**: Contains all the material information		
 - std::vector<MeshGroup> **Groups**: Contains all the groups of this mesh(if no groups  defined, it will just store the range of the full **Indicies** vector)

### MeshGroup
####	The MeshGroup is like a portion of the original mesh, storing the range of the indicies that builds it up, in a from index - to index way. So you can simple use it with glDrawElements, just by passing the start as offset and the count.
	
 - std::string **Name**: Name of ther group
 - uint32_t **FirstIndex**: Where it starts from in the Indicies list: Mesh.Indicies[FirstIndex] ...
 - uint32_t **LastIndex**: The last included "index" index : Mesh.Indicies[LastIndex]
 - uint32_t **IndicesCount**: Count of the indicies in the group, basicly **LastIndex - FirstIndex + 1**
 - Not final but works: std::vector<std::pair<uint32_t, std::string>> **FaceMaterials**: Stores the change of materials in a key/value pair. First integer is the index when this material is active.
			- Example List: <0, "Gold">, <6, "Blue">, indicies/triangles has the "Gold" material from Index 0-5 from index 6 we are using the Blue material for rendering

### Material
Stores all the standard material properties.


## Usage:
	// lets load our .obj file, !must end with lowercase .obj!
	QuickObjectLoader::Loader obj("my.obj"); 
	const auto mesh = obj.GetMeshes().front(); // now we just grab the first mesh, hopefully it worked :P
	
	// Loading indexes into the index buffer:
	glDrawElements(GL_TRIANGLES, ib.GetCount(), GL_UNSIGNED_INT, reinterpret_cast<const void*>(ib.GetStart()));

	// loading indexes into the vertex buffer
	glBufferData(GL_ARRAY_BUFFER, mesh.Vertices.size(), mesh.Vertices.data(), GL_STATIC_DRAW);
	
	// Drawing all elements
	glDrawElements(GL_TRIANGLES, mesh.Indicies.size(), GL_UNSIGNED_INT, 0);
	
	// Drawing only a group
	glDrawElements(GL_TRIANGLES, mesh.Groups.front().FirstIndex, GL_UNSIGNED_INT, mesh.Groups.front().IndicesCount);
