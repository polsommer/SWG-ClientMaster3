// ======================================================================
//
// TemporaryCrcString.cpp
// Copyright 2002, Sony Online Entertainment Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedFoundation/FirstSharedFoundation.h"
#include "sharedFoundation/TemporaryCrcString.h"

#include "sharedFoundation/Os.h"
#include "sharedFoundation/Crc.h"

// ======================================================================

TemporaryCrcString::TemporaryCrcString()
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	m_buffer[0] = '\0';
}

// ----------------------------------------------------------------------
/**
 * Copy constructor.
 *
 * TRF needed this to allow useful sanity checking with std::set<TemporaryCrcString>.
 */
TemporaryCrcString::TemporaryCrcString(const TemporaryCrcString &rhs)
: CrcString()
{
	set(rhs.getString(), rhs.getCrc());
}

// ----------------------------------------------------------------------

TemporaryCrcString::TemporaryCrcString(char const * string, bool applyNormalize)
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	set(string, applyNormalize);
}

// ----------------------------------------------------------------------

TemporaryCrcString::TemporaryCrcString(char const * string, uint32 crc)
: CrcString()
{
	// this is to avoid including Os in the header file
	DEBUG_FATAL(static_cast<int>(BUFFER_SIZE) != static_cast<int>(Os::MAX_PATH_LENGTH), ("Os::MAX_PATH_LENGTH and BUFFER_SIZE differ"));

	set(string, crc);
}

// ----------------------------------------------------------------------

TemporaryCrcString::~TemporaryCrcString()
{
}

// ----------------------------------------------------------------------

char const * TemporaryCrcString::getString() const
{
	return m_buffer;
}

// ----------------------------------------------------------------------

void TemporaryCrcString::clear()
{
	m_buffer[0] = '\0';
	m_crc = Crc::crcNull;
}

// ----------------------------------------------------------------------

namespace
{
        // Safely copies the supplied string into the destination buffer while guaranteeing
        // null-termination.  Returns true if the input string was truncated in order to fit
        // within the destination buffer.
        bool copyStringTruncate(char *destination, size_t destinationSize, char const *source)
        {
                if (destinationSize == 0)
                        return false;

                size_t index = 0;
                for (; index + 1 < destinationSize && source[index] != '\0'; ++index)
                        destination[index] = source[index];

                destination[index] = '\0';
                return source[index] != '\0';
        }

        // Normalizes the supplied string into the destination buffer while guaranteeing
        // null-termination.  The behavior mirrors CrcString::normalize but adds bounds
        // checking so that overly long normalized strings are truncated safely.  Returns
        // true if truncation was required.
        bool normalizeStringTruncate(char *destination, size_t destinationSize, char const *source)
        {
                if (destinationSize == 0)
                        return false;

                size_t index = 0;
                bool previousIsSlash = true;
                bool truncated = false;

                for ( ; *source != '\0'; ++source)
                {
                        char outputCharacter = '\0';
                        bool writeCharacter = false;

                        const char c = *source;
                        if (c == '\\' || c == '/')
                        {
                                if (!previousIsSlash)
                                {
                                        outputCharacter = '/';
                                        writeCharacter = true;
                                        previousIsSlash = true;
                                }
                        }
                        else if (c == '.')
                        {
                                if (!previousIsSlash)
                                {
                                        outputCharacter = '.';
                                        writeCharacter = true;
                                }
                        }
                        else
                        {
                                outputCharacter = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                                writeCharacter = true;
                                previousIsSlash = false;
                        }

                        if (writeCharacter)
                        {
                                if (index + 1 < destinationSize)
                                {
                                        destination[index++] = outputCharacter;
                                }
                                else
                                {
                                        truncated = true;
                                        break;
                                }
                        }
                }

                destination[index] = '\0';

                if (!truncated && *source != '\0')
                        truncated = true;

                return truncated;
        }
}

void TemporaryCrcString::internalSet(char const * string, bool applyNormalize)
{
        bool truncated = false;

        if (applyNormalize)
        {
                truncated = normalizeStringTruncate(m_buffer, sizeof(m_buffer), string);
        }
        else
        {
                truncated = copyStringTruncate(m_buffer, sizeof(m_buffer), string);
        }

#ifdef _DEBUG
        DEBUG_FATAL(truncated, ("string too long %d/%d", static_cast<int>(strlen(string)) + 1, BUFFER_SIZE));
#else
        if (truncated)
                WARNING(true, ("TemporaryCrcString truncated string [%s] to %d characters", string, BUFFER_SIZE - 1));
#endif
}

// ----------------------------------------------------------------------

void TemporaryCrcString::set(char const * string, bool applyNormalize)
{
	NOT_NULL(string);
	internalSet(string, applyNormalize);
	calculateCrc();
}

// ----------------------------------------------------------------------

void TemporaryCrcString::set(char const * string, uint32 crc)
{
	NOT_NULL(string);
	internalSet(string, false);
	m_crc = crc;
}

// ======================================================================
