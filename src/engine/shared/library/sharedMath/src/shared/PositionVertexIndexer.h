// ======================================================================
//
// PositionVertexIndexer.h
// Copyright 2004, Sony Online Entertainment
//
// ======================================================================

#ifndef INCLUDED_PositionVertexIndexer_H
#define INCLUDED_PositionVertexIndexer_H

// ======================================================================

#include "sharedMath/Vector.h"

<<<<<<< Updated upstream
<<<<<<< Updated upstream
#include "sharedFoundation/HashMap.h"
=======
#include <unordered_map>
>>>>>>> Stashed changes
=======
#include <unordered_map>
>>>>>>> Stashed changes

// ======================================================================

class PositionVertexIndexer
{
public:
	typedef std::vector<Vector> VectorVector;

	PositionVertexIndexer();
	~PositionVertexIndexer();

	void clear();
	void reserve(int numberOfVertices);
	int addVertex(Vector const & vertex);
	int getNumberOfVertices() const;

	Vector const & getVertex(int index) const;
	VectorVector const & getVertices() const;

	Vector & getVertex(int index);
	VectorVector & getVertices();

private:

	PositionVertexIndexer(PositionVertexIndexer const &);
	PositionVertexIndexer & operator=(PositionVertexIndexer const &);

private:

	typedef std::unordered_multimap<uint32 /*crc*/, int /*index*/> VertexIndexMap;

	VectorVector * m_vertices;
	VertexIndexMap * m_indexMap;
};

// ======================================================================

#endif
