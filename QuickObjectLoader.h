#pragma once

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

		// Illummination
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
		size_t FirstIndex{};
		size_t IndicesCount{};
		std::vector<std::pair<uint32_t, std::string>> FaceMaterials{};
	};

	struct MeshObject {
		std::string Name{};
		std::vector<MeshGroup> Groups{};
		size_t FirstIndex{};
		size_t IndicesCount{};
	};

	struct Mesh
	{
		std::string Name{};
		std::vector<Vertex> Vertices{};
		std::vector<uint32_t> Indices{};
		std::vector<MeshObject> Objects{};
		std::vector<Material> Materials{};
	};

	class Loader
	{
	public:

		Loader() {}
		Loader(const std::string_view& path) { LoadFile(path); }

		auto& GetMesh() const { return _mesh; }

		bool LoadFile(const std::string_view& path)
		{
			_mesh = {};

			if (path.substr(path.size() - 4, 4) != ".obj")
				return false;

			std::ifstream file(path.data(), std::ios_base::ate);

			if (!file.is_open())
				return false;

			const auto length = file.tellg();
			std::vector<char> content(length);
			file.seekg(0);
			file.read(content.data(), length);
			file.close();

			std::vector<glm::vec2> texCoords;
			std::vector<glm::vec3> normals;
			std::vector<std::string_view> temp(30);
			
			_mesh.Objects.emplace_back();
			_mesh.Objects.front().Groups.emplace_back();

			auto* currentObject = &_mesh.Objects.front();
			auto* currentGroup = &currentObject->Groups.front();

			uint32_t lineStart = 0, lineEnd = 0;

			while (lineEnd < content.size())
			{
				lineStart = lineEnd;

				while (content[lineEnd] != '\n' && lineEnd < content.size() - 1) lineEnd++;

				const auto line = std::string_view(&content[lineStart], lineEnd - lineStart);

				if (line.empty() || line[0] == '#' || line[0] == 0) {
					++lineEnd;
					continue;
				}

				const auto firstToken = GetFirstToken(line);
				auto start = firstToken.length() + 1;
				for (auto i = start; i < line.length(); i++) {
					if (!std::isspace(line[i])) {
						start = i;
						break;
					}
				}

				const auto curline = line.substr(start);

				// Generate a Mesh Object or Prepare for an object to be created
				if (!firstToken.compare("o") || !firstToken.compare("g"))
				{
					const auto name = curline.length() > 2 ? std::string(curline) : std::string("unnamed");

					if (!firstToken.compare("o")) {
						if (currentObject->IndicesCount == 0) {
							currentObject->Name = name;
							currentObject->FirstIndex = _mesh.Indices.size();
							currentGroup->FirstIndex = _mesh.Indices.size();
							currentGroup->IndicesCount = 0;
						}
						else {
							_mesh.Objects.emplace_back(name);
							currentObject = &_mesh.Objects.back();

							currentObject->FirstIndex = _mesh.Indices.size();

							currentObject->Groups.emplace_back();
							currentGroup = &currentObject->Groups.back();
							currentGroup->FirstIndex = _mesh.Indices.size();
						}
					}
					else {
						if (currentGroup->IndicesCount == 0) {
							currentGroup->Name = name;
						}
						else {
							currentObject->Groups.emplace_back();
							currentGroup = &currentObject->Groups.back();
							const auto index = _mesh.Indices.size();
							currentGroup->Name = name;
							currentGroup->FirstIndex = index;
						}
					}
				}
				else if (!firstToken.compare("v")) // Generate a Vertex Position
				{
					SplitByWhitespace(curline, temp);
					_mesh.Vertices.push_back({ { ParseFloat(temp[0]), ParseFloat(temp[1]), ParseFloat(temp[2]) } });
				}
				else if (!firstToken.compare("vt")) 				// Generate a Vertex Texture Coordinate
				{
					SplitByWhitespace(curline, temp);
					texCoords.emplace_back(ParseFloat(temp[0]), ParseFloat(temp[1]));
				}
				else if (!firstToken.compare("vn")) // Generate a Vertex Normal;
				{
					SplitByWhitespace(curline, temp);
					normals.push_back({ ParseFloat(temp[0]), ParseFloat(temp[1]), ParseFloat(temp[2]) });
				}
				else if (!firstToken.compare("f")) // Generate a Face (vertices & indices)
				{
					BuildIndicies(*currentObject, *currentGroup, texCoords, normals, curline);
				}
				else if (!firstToken.compare("usemtl")) // Get Mesh Material Name
				{
					currentGroup->FaceMaterials.push_back({ _mesh.Indices.size(), std::string(curline)});
				}
				else if (!firstToken.compare("mtllib")) // Load Materials
				{
					Split<'/'>(path, temp);

					std::string pathtomat;

					if (temp.size() != 1)
					{
						for (int i = 0; i < temp.size() - 1; i++) {
							pathtomat.append(temp[i]);
							pathtomat.append("/");
						}
					}

					pathtomat += std::string(curline);

					LoadMaterials(pathtomat, _mesh.Materials);
				}

				++lineEnd;
			}

			if (currentGroup->IndicesCount == 0)
				_mesh.Objects.back().Groups.pop_back();

			if (currentObject->IndicesCount == 0)
				_mesh.Objects.pop_back();

			return !_mesh.Objects.empty();
		}

	private:
		Mesh _mesh;
		std::vector<std::string_view> _tempFaces = std::vector<std::string_view>(200);
		std::vector<std::string_view> _tempVerticies = std::vector<std::string_view>(200);
		std::vector<uint32_t> _tempIndicies = std::vector<uint32_t>(200);

		template <class T>
		const T& GetElement(const std::vector<T>& elements, const std::string_view& index) { return elements[ParseInt(index) - 1]; }

		void BuildIndicies(MeshObject& currentMeshObject, MeshGroup& currentMeshGroup, std::vector<glm::vec2>& texCoords, std::vector<glm::vec3>& normals, const std::string_view& currentLine)
		{
			_tempFaces.clear();
			_tempVerticies.clear();

			SplitByWhitespace(currentLine, _tempFaces);

			for (auto i = 0; i < _tempFaces.size(); i++)
			{
				Split<'/'>(_tempFaces[i], _tempVerticies);
				int32_t vtype{};

				// Check for just position - v1
				if (_tempFaces.size() == 1) // Only position
					vtype = 1;
				else if (_tempVerticies.size() == 2) // Check for position & texture - v1/vt1
					vtype = 2;
				else if (_tempVerticies.size() == 3) // Check for Position, Texture and Normal - v1/vt1/vn1 // or if Position and Normal - v1//vn1
					vtype = !_tempVerticies[1].empty() ? 4 : 3; // Position, Texture, and Normal OR  Position & Normal

				auto vertexIndex = static_cast<uint32_t>(ParseInt(_tempVerticies[0]) - 1);
				auto &vertex = _mesh.Vertices[vertexIndex];
				
				_tempIndicies[i] = vertexIndex;
				
				switch (vtype)
				{
				case 2: // P/T
				{
					vertex.TextureCoordinate = GetElement(texCoords, _tempVerticies[1]);
					break;
				}
				case 3: // P//N
				{
					vertex.Normal = GetElement(normals, _tempVerticies[2]);
					break;
				}
				case 4: // P/T/N
				{
					vertex.TextureCoordinate = GetElement(texCoords, _tempVerticies[1]);
					vertex.Normal = GetElement(normals, _tempVerticies[2]);
					break;
				}
				default:
					break;
				}
			}

			if (_tempFaces.size() <= 3) {
				for (auto i = 0; i < _tempFaces.size(); i++)
					_mesh.Indices.emplace_back(_tempIndicies[i]);

				currentMeshGroup.IndicesCount += _tempFaces.size();
				currentMeshObject.IndicesCount += _tempFaces.size();
			}
			else {
				const auto firstIndex = _tempIndicies[0];

				//generate fans
				for (auto i = 1; i < _tempFaces.size() - 1; i++) {
					_mesh.Indices.emplace_back(firstIndex);
					_mesh.Indices.emplace_back(_tempIndicies[i]);
					_mesh.Indices.emplace_back(_tempIndicies[i+1]);
				}

				const auto indiciesCount = (_tempFaces.size() - 2) * 3;
				currentMeshGroup.IndicesCount += indiciesCount;
				currentMeshObject.IndicesCount += indiciesCount;
			}
		}

		inline std::string ReplaceAll(std::string& str, const char from, const char to) {
			size_t start_pos = 0;

			while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
				str.replace(std::begin(str), std::end(str), from, to);
				start_pos += 1; // Handles case where 'to' is a substring of 'from'
			}
			return str;
		}

		inline int32_t ParseInt(const std::string_view& str) {
			if (str.empty()) return 0;
			int32_t value;
			std::from_chars(str.data(), str.data() + str.size(), value);
			return value;
		}

		inline float ParseFloat(const std::string_view& str) {
			if (str.empty()) return 0.f;
			float value;
			std::from_chars(str.data(), str.data() + str.size(), value);
			return value;
		}
		
		template<char token>
		void Split(const std::string_view& in, std::vector<std::string_view>& out)
		{
			out.clear();

			size_t last_position{};
			const auto bf = in.data();
			const auto length = in.length();

			while (last_position < length) {
				auto end_pos = SIZE_MAX;

				for (auto i = last_position; i < length; i++) {
					if (bf[i] == token) {
						end_pos = i;
						break;
					}
				}

				if (end_pos == SIZE_MAX) end_pos = in.size();
			
				out.push_back(in.substr(last_position, end_pos - last_position));
				
				last_position = end_pos + 1;
			}
		}

		void SplitByWhitespace(const std::string_view& in, std::vector<std::string_view>& out)
		{
			out.clear();

			size_t start_pos{};
			const auto bf = in.data();
			const auto length = in.length();

			while (start_pos < length) {
				auto end_pos = SIZE_MAX;

				for (auto i = start_pos; i < length; i++) {
					if (std::isspace(bf[i])) {
						end_pos = i;
						break;
					}
				}	

				if (end_pos == SIZE_MAX) end_pos = in.size();
				else if (end_pos == start_pos && std::isspace(bf[end_pos])) {
					start_pos = end_pos + 1;
					continue;
				}
				out.push_back(in.substr(start_pos, end_pos - start_pos - 1));
				start_pos = end_pos;
			}
		}

		std::string_view GetFirstToken(const std::string_view& in)
		{
			if (in.empty()) return {};

			const auto token_start = in.find_first_not_of(" \t");
			const auto token_end = in.find_first_of(" \t", token_start);

			if (token_start != std::string_view::npos && token_end != std::string_view::npos)
				return in.substr(token_start, token_end - token_start);

			if (token_start != std::string_view::npos)
				return in.substr(token_start);

			return {};
		}
		
		bool LoadMaterials(std::string path, std::vector<Material>& materials)
		{
			// If the file is not a material file return false
			if (path.substr(path.size() - 4, path.size()) != ".mtl")
				return false;

			path = ReplaceAll(path, '\\', '/');
			const auto lastSlash = path.find_last_of('/');
			const std::string rootPath = (lastSlash != std::string::npos) ? path.substr(0, lastSlash + 1) : std::string();

			std::ifstream file(path.data(), std::ios_base::ate);

			if (!file.is_open())
				return false;

			const auto length = file.tellg();
			std::vector<char> content(length);
			file.seekg(0);
			file.read(content.data(), length);
			file.close();
			
			materials.emplace_back();
			auto* currentMat = &materials.front();

			std::vector<std::string_view> temp(50);

			uint32_t lineStart = 0, lineEnd = 0;

			while (lineEnd < content.size())
			{
				lineStart = lineEnd;

				while (content[lineEnd] != '\n' && lineEnd < content.size() - 1) lineEnd++;

				const auto line = std::string_view(&content[lineStart], lineEnd - lineStart);

				++lineEnd;

				if (line.empty() || line[0] == '#' || line[0] == 0)
					continue;

				const auto firstToken = GetFirstToken(line);
				const auto curline = line.substr(firstToken.length() + 1);
				if (!firstToken.compare("newmtl")) // new material and material name
				{
					const auto name = curline.size() > 7 ? std::string(curline) : "none";
					
					if (currentMat->Name.size() != 0) {
						materials.emplace_back();
						currentMat = &materials.back();
					}

					currentMat->Name = name;
				}
				else if (!firstToken.compare("Ka"))  // Ambient Color
				{
					SplitByWhitespace(curline, temp);

					if (temp.size() != 3)
						continue;

					currentMat->Ka.x = ParseFloat(temp[0]);
					currentMat->Ka.y = ParseFloat(temp[1]);
					currentMat->Ka.z = ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Kd")) // Diffuse Color
				{
					SplitByWhitespace(curline, temp);

					if (temp.size() != 3)
						continue;

					currentMat->Kd.x = ParseFloat(temp[0]);
					currentMat->Kd.y = ParseFloat(temp[1]);
					currentMat->Kd.z = ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Ks")) // Specular Color
				{
					SplitByWhitespace(curline, temp);

					if (temp.size() != 3)
						continue;

					currentMat->Ks.x = ParseFloat(temp[0]);
					currentMat->Ks.y = ParseFloat(temp[1]);
					currentMat->Ks.z = ParseFloat(temp[2]);
				}
				else if (!firstToken.compare("Ns")) // Specular Exponent
					currentMat->Ns = ParseFloat(curline);
				else if (!firstToken.compare("Ni")) // Optical Density
					currentMat->Ni = ParseFloat(curline);
				else if (!firstToken.compare("d")) // Dissolve
					currentMat->d = ParseFloat(curline);
				else if (!firstToken.compare("illum")) // Illumination
					currentMat->illum = ParseInt(curline);
				else if (!firstToken.compare("map_Ka")) // Ambient Texture Map
					currentMat->map_Ka = curline;
				else if (!firstToken.compare("map_Kd")) // Diffuse Texture Map
					currentMat->map_Kd = curline;
				else if (!firstToken.compare("map_Ks")) // Specular Texture Map
					currentMat->map_Ks = curline;
				else if (!firstToken.compare("map_Ns")) // Specular Hightlight Map
					currentMat->map_Ns = curline;
				else if (!firstToken.compare("map_d")) // Alpha Texture Map
					currentMat->map_d = rootPath + std::string(curline);
				else if (!firstToken.compare("map_Bump") || !firstToken.compare("map_bump") || !firstToken.compare("bump")) // Bump Map
					currentMat->map_bump = std::string(curline);
			}

			return true;
		}
	};
}