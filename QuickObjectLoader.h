#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <charconv>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

namespace QuickObjectLoader
{
	struct Vertex {
		glm::vec3 Position{};
		glm::vec2 TextureCoordinate{};
		glm::vec3 Normal{};
		constexpr Vertex() {}
		constexpr Vertex(const glm::vec3& position) : Position(position) {}
		constexpr Vertex(const glm::vec3& position, const glm::vec2& tex) : Position(position), TextureCoordinate(tex) {}
	};

	struct Material
	{
		std::string Name{};

		// Ambient Color
		glm::vec3 Ka{};

		// Diffuse Color
		glm::vec3 Kd{};

		// Specular Color
		glm::vec3 Ks{};

		// Specular Exponent
		float Ns{};

		// Optical Density
		float Ni{};

		// Dissolve
		float d{};

		// Illumination
		int32_t illum{};

		// Ambient Texture Map
		std::string map_Ka{};

		// Diffuse Texture Map
		std::string map_Kd{};

		// Specular Texture Map
		std::string map_Ks{};

		// Specular Hightlight Map
		std::string map_Ns{};

		// Alpha Texture Map
		std::string map_d{};

		// Bump Map
		std::string map_bump{};
	};

	struct MeshGroup {
		std::string Name{};
		uint32_t FirstIndex{};
		uint32_t LastIndex{};
		uint32_t IndicesCount{};
		std::vector<std::pair<uint32_t, std::string>> FaceMaterials{};
		MeshGroup() :Name({}), FirstIndex(0), LastIndex(0), IndicesCount(0) { }
	};

	struct Mesh
	{
		std::string Name{};
		std::vector<Vertex> Vertices{};
		std::vector<uint32_t> Indices{};
		std::vector<Material> Materials{};
		std::vector<MeshGroup> Groups{};

		Mesh() noexcept { }
		Mesh(const std::string name) : Name(name) {}
	};

	namespace alg
	{
		inline int32_t ParseInt(const std::string_view& str) {
			if (str.empty())
				return 0;

			int32_t value;
			std::from_chars(str.data(), str.data() + str.size(), value);
			return value;
		}

		inline float ParseFloat(const std::string_view& str) {
			if (str.empty())
				return 0.f;

			float value;
			std::from_chars(str.data(), str.data() + str.size(), value);
			return value;
		}
	
		// Split a String into a string array at a given token
		void Split(const std::string_view& in, std::vector<std::string_view>& out, std::string_view token)
		{
			out.clear();
			size_t last_position{};

			while (last_position < in.length()) {

				auto end_pos = in.find_first_of(token, last_position);

				if (end_pos == SIZE_MAX)
					end_pos = in.size();

				out.push_back(in.substr(last_position, end_pos - last_position));

				last_position = end_pos + 1;
			}
		}

		// Get tail of string after first token and possibly following spaces
		template <typename T>
		inline T tail(const std::string_view& in)
		{
			size_t token_start = in.find_first_not_of(" \t");
			size_t space_start = in.find_first_of(" \t", token_start);
			size_t tail_start = in.find_first_not_of(" \t", space_start);
			size_t tail_end = in.find_last_not_of(" \t");
			if (tail_start != std::string::npos && tail_end != std::string::npos)
				return T(in.substr(tail_start, tail_end - tail_start + 1));

			if (tail_start != std::string::npos)
				return T(in.substr(tail_start));

			return {};
		}

		// Get first token of string
		std::string_view GetFirstToken(const std::string_view& in)
		{
			if (in.empty())
				return {};

			const auto token_start = in.find_first_not_of(" \t");
			const auto token_end = in.find_first_of(" \t", token_start);

			if (token_start != std::string_view::npos && token_end != std::string_view::npos)
				return std::string_view(in.substr(token_start, token_end - token_start));

			if (token_start != std::string_view::npos)
				return std::string_view(in.substr(token_start));

			return {};
		}

		// Get element at given index position
		template <class T>
		inline const T& GetElement(const std::vector<T>& elements, const std::string_view& index)
		{
			auto idx = ParseInt(index);

			if (idx < 0)
				idx = static_cast<int>(elements.size()) + idx;
			else
				idx--;

			return elements[idx];
		}
	}

	class Loader
	{
	public:

		Loader() {}

		Loader(const std::string_view& path) {
			LoadFile(path);
		}

		auto& GetMeshes() const { return _meshes; }

		// Load a file into the loader
		//
		// If file is loaded return true
		//
		// If the file is unable to be found
		// or unable to be loaded return false
		bool LoadFile(const std::string_view& path)
		{
			_meshes.clear();

			if (path.substr(path.size() - 4, 4) != ".obj")
				return false;

			
			std::ifstream file(path.data(), std::ios_base::ate);

			if (!file.is_open())
				return false;

			const uint32_t length = file.tellg();
			std::vector<char> content(length);
			file.seekg(0);
			file.read(content.data(), length);
			file.close();

			_meshes.emplace_back();
			_meshes.front().Groups.emplace_back();

			auto* currentMesh = &_meshes[0];
			auto* currentGroup = &_meshes[0].Groups[0];

			std::vector<std::string_view> temp(20);
			std::string tempStrLine;
			std::vector<glm::vec2> texCoords;
			std::vector<glm::vec3> normals;

			uint32_t lineStart = 0, lineEnd = 0;

			while (lineEnd < content.size())
			{
				lineStart = lineEnd;

				while (content[lineEnd] != '\n' && lineEnd < content.size() - 1) lineEnd++;

				const auto curline = std::string_view(&content[lineStart], lineEnd - lineStart);

				if (curline.empty() || curline[0] == '#') {
					++lineEnd;
					continue;
				}

				const auto firstToken = alg::GetFirstToken(curline);

				// Generate a Mesh Object or Prepare for an object to be created
				if (!firstToken.compare("o") || !firstToken.compare("g"))
				{
					const auto name = curline.length() > 2 ? alg::tail<std::string>(curline) : std::string("unnamed");

					if (!firstToken.compare("o")) {
						if (currentMesh->Indices.size() == 0 || currentMesh->Vertices.size() == 0) {
							currentMesh->Name = name;
							currentGroup->FirstIndex = 0;
							currentGroup->LastIndex = 0;
							currentGroup->IndicesCount = 0;
						}
						else {
							_meshes.emplace_back(name);
							currentMesh = &_meshes[_meshes.size() - 1];
							currentMesh->Groups.emplace_back();
							currentGroup = &currentMesh->Groups[currentMesh->Groups.size() - 1];
						}
					}
					else {
						if (currentGroup->IndicesCount == 0) {
							currentGroup->Name = name;
						}
						else {
							currentMesh->Groups.emplace_back();
							currentGroup = &currentMesh->Groups[currentMesh->Groups.size() - 1];
							const auto index = currentMesh->Indices.size();
							currentGroup->Name = name;
							currentGroup->FirstIndex = index;
							currentGroup->LastIndex = index;
						}

					}
				}
				else if (!firstToken.compare("v")) // Generate a Vertex Position
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");
					currentMesh->Vertices.push_back({ { alg::ParseFloat(temp[0]), alg::ParseFloat(temp[1]), alg::ParseFloat(temp[2]) } });
				}
				else if (!firstToken.compare("vt")) 				// Generate a Vertex Texture Coordinate
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");
					texCoords.emplace_back(alg::ParseFloat(temp[0]), alg::ParseFloat(temp[1]));
				}
				else if (!firstToken.compare("vn")) // Generate a Vertex Normal;
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");
					normals.push_back({ alg::ParseFloat(temp[0]), alg::ParseFloat(temp[1]), alg::ParseFloat(temp[2]) });
				}
				else if (!firstToken.compare("f")) // Generate a Face (vertices & indices)
				{
					BuildIndicies(*currentMesh, currentGroup, texCoords, normals, curline);
				}
				else if (!firstToken.compare("usemtl")) // Get Mesh Material Name
				{
					const auto& matName = alg::tail<std::string_view>(curline);
					currentGroup->FaceMaterials.emplace_back(currentMesh->Indices.size(), matName);
				}
				else if (!firstToken.compare("mtllib")) // Load Materials
				{
					alg::Split(path, temp, "/");

					std::string pathtomat;

					if (temp.size() != 1)
					{
						for (int i = 0; i < temp.size() - 1; i++) {
							pathtomat.append(temp[i]);
							pathtomat.append("/");
						}
					}

					pathtomat += alg::tail<std::string>(curline);

					LoadMaterials(pathtomat, currentMesh->Materials);
				}

				++lineEnd;
			}


			if (currentMesh->Indices.size() == 0 && currentMesh->Vertices.size() == 0)
				_meshes.pop_back();

			return !_meshes.empty();
		}

	private:
		std::vector<Mesh> _meshes;

		void BuildIndicies(Mesh& mesh, MeshGroup* currentGroup, const std::vector<glm::vec2>& texCoords, const std::vector<glm::vec3>& normals, const std::string_view& currentLine)
		{
			std::vector<std::string_view> sface, svert;
			alg::Split(alg::tail<std::string_view>(currentLine), sface, " ");
			
			auto stackIndexArray = (uint32_t*)alloca(sizeof(uint32_t) * sface.size());

			for (auto i = 0; i < sface.size(); i++)
			{
				alg::Split(sface[i], svert, "/");
				int32_t vtype{};

				// Check for just position - v1
				if (svert.size() == 1) // Only position
					vtype = 1;
				else if (svert.size() == 2) // Check for position & texture - v1/vt1
					vtype = 2;
				else if (svert.size() == 3) // Check for Position, Texture and Normal - v1/vt1/vn1 // or if Position and Normal - v1//vn1
					vtype = !svert[1].empty() ? 4 : 3; // Position, Texture, and Normal OR  Position & Normal

				auto vertex = alg::GetElement(mesh.Vertices, svert[0]);

				stackIndexArray[i] = static_cast<uint32_t>(alg::ParseInt(svert[0]) - 1);
				
				switch (vtype)
				{
				case 2: // P/T
				{
					vertex.TextureCoordinate = alg::GetElement(texCoords, svert[1]);
					break;
				}
				case 3: // P//N
				{
					vertex.Normal = alg::GetElement(normals, svert[2]);
					break;
				}
				case 4: // P/T/N
				{
					vertex.TextureCoordinate = alg::GetElement(texCoords, svert[1]);
					vertex.Normal = alg::GetElement(normals, svert[2]);
					break;
				}
				default:
					break;
				}
			}

			if (sface.size() <= 3) {
				for (auto i = 0; i < sface.size(); i++)
					mesh.Indices.emplace_back(stackIndexArray[i]);

				currentGroup->LastIndex += sface.size();
				currentGroup->IndicesCount += sface.size();
			}
			else {
				const auto firstIndex = stackIndexArray[0];

				//generate fans
				for (auto i = 1; i < sface.size() - 1; i++) {
					mesh.Indices.emplace_back(firstIndex);
					mesh.Indices.emplace_back(stackIndexArray[i]);
					mesh.Indices.emplace_back(stackIndexArray[i+1]);
				}

				currentGroup->LastIndex += (sface.size() - 2) *3 ;
				currentGroup->IndicesCount += (sface.size() - 2) * 3;
			}
		}
		
		// Load Materials from .mtl file
		bool LoadMaterials(std::string path, std::vector<Material>& materials)
		{
			// If the file is not a material file return false
			if (path.substr(path.size() - 4, path.size()) != ".mtl")
				return false;

			std::ifstream file(path);

			// If the file is not found return false
			if (!file.is_open())
				return false;

			Material & currentMat = materials.emplace_back();
			std::vector<std::string_view> temp(50);
			auto first = true;

			// Go through each line looking for material variables
			std::string curStrLine;
			while (std::getline(file, curStrLine))
			{
				const auto curline = std::string_view(curStrLine);
				const auto firstToken = alg::GetFirstToken(curline);

				temp.clear();

				if (!firstToken.compare("newmtl")) // new material and material name
				{
					const auto name = curline.size() > 7 ? alg::tail<std::string>(curline) : "none";

					if (!first)
						currentMat = materials.emplace_back();
					else
						first = false;

					currentMat.Name = name;
				}
				else if (!firstToken.compare("Ka"))  // Ambient Color
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");

					if (temp.size() != 3)
						continue;

					currentMat.Ka.x = alg::ParseFloat(temp[0]);
					currentMat.Ka.y = alg::ParseFloat(temp[1]);
					currentMat.Ka.z = alg::ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Kd")) // Diffuse Color
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");

					if (temp.size() != 3)
						continue;

					currentMat.Kd.x = alg::ParseFloat(temp[0]);
					currentMat.Kd.y = alg::ParseFloat(temp[1]);
					currentMat.Kd.z = alg::ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Ks")) // Specular Color
				{
					alg::Split(alg::tail<std::string_view>(curline), temp, " ");

					if (temp.size() != 3)
						continue;

					currentMat.Ks.x = alg::ParseFloat(temp[0]);
					currentMat.Ks.y = alg::ParseFloat(temp[1]);
					currentMat.Ks.z = alg::ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Ns")) // Specular Exponent
					currentMat.Ns = alg::ParseFloat(alg::tail<std::string_view>(curline));
				else if (!firstToken.compare("Ni")) // Optical Density
					currentMat.Ni = alg::ParseFloat(alg::tail<std::string_view>(curline));
				else if (!firstToken.compare("d")) // Dissolve
					currentMat.d = alg::ParseFloat(alg::tail<std::string_view>(curline));
				else if (!firstToken.compare("illum")) // Illumination
					currentMat.illum = alg::ParseInt(alg::tail<std::string_view>(curline));
				else if (!firstToken.compare("map_Ka")) // Ambient Texture Map
					currentMat.map_Ka = alg::tail<std::string>(curline);
				else if (!firstToken.compare("map_Kd")) // Diffuse Texture Map
					currentMat.map_Kd = alg::tail<std::string>(curline);
				else if (!firstToken.compare("map_Ks")) // Specular Texture Map
					currentMat.map_Ks = alg::tail<std::string>(curline);
				else if (!firstToken.compare("map_Ns")) // Specular Hightlight Map
					currentMat.map_Ns = alg::tail<std::string>(curline);
				else if (!firstToken.compare("map_d")) // Alpha Texture Map
					currentMat.map_d = alg::tail<std::string>(curline);
				else if (!firstToken.compare("map_Bump") || !firstToken.compare("map_bump") || !firstToken.compare("bump")) // Bump Map
					currentMat.map_bump = alg::tail<std::string>(curline);
			}

			return true;
		}
	};
}