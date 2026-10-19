// The streaming buffer: per-draw constants, 2D vertices, immediate meshes and quad indices. One
// buffer of kStreamSegments segments, each the data of one frame in flight; a frame's segment is
// fenced when the frame is submitted, and the next frame waits for the segment it takes over.
//
// Persistently mapped (GL 4.4's ARB_buffer_storage) where the context has it: the CPU writes where
// the GPU reads, with no copy. Otherwise a segment is staged in memory and uploaded with
// glBufferSubData before each GL call that reads it.

#include <algorithm>
#include <cstring>

#include "gl.hpp"

namespace gfx::detail {

namespace {

constexpr GLsizeiptr kInitialSegment = GLsizeiptr{8} << 20;

bool HasExtension(const char *name) {
    GLint count = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &count);
    for (GLint i = 0; i < count; i++) {
        const char *extension = reinterpret_cast<const char *>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (extension != nullptr && std::strcmp(extension, name) == 0) {
            return true;
        }
    }
    return false;
}

GLsizeiptr SegmentSize() { return gl.stream_size / kStreamSegments; }

void WaitSegment(uint32_t segment) {
    GLsync &fence = gl.segment_fences[segment];
    if (fence != nullptr) {
        glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, GL_TIMEOUT_IGNORED);
        glDeleteSync(fence);
        fence = nullptr;
    }
}

void CreateStream(GLsizeiptr segment_size) {
    gl.stream_size = segment_size * kStreamSegments;
    glGenBuffers(1, &gl.stream);
    glBindBuffer(GL_ARRAY_BUFFER, gl.stream);
    if (gl.stream_persistent) {
        GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
        glBufferStorage(GL_ARRAY_BUFFER, gl.stream_size, nullptr, flags);
        gl.stream_data = static_cast<uint8_t *>(glMapBufferRange(GL_ARRAY_BUFFER, 0, gl.stream_size, flags));
        if (gl.stream_data == nullptr) {
            Fatal("GL: the streaming buffer cannot be mapped");
        }
    } else {
        glBufferData(GL_ARRAY_BUFFER, gl.stream_size, nullptr, GL_STREAM_DRAW);
        gl.staging.assign(static_cast<size_t>(segment_size), 0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    gl.segment_fences.assign(kStreamSegments, nullptr);
    gl.segment = 0;
    gl.segment_used = 0;
    gl.staged_from = 0;
    // The vertex arrays name the buffer as their element buffer; it is a new one.
    for (GLuint &buffer : gl.bound.element_buffer) {
        buffer = 0;
    }
    for (GLuint &buffer : gl.bound.vertex_buffer) {
        buffer = 0;
    }
}

void DestroyStream() {
    for (uint32_t segment = 0; segment < gl.segment_fences.size(); segment++) {
        WaitSegment(segment);
    }
    if (gl.stream != 0) {
        if (gl.stream_persistent) {
            glBindBuffer(GL_ARRAY_BUFFER, gl.stream);
            glUnmapBuffer(GL_ARRAY_BUFFER);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glDeleteBuffers(1, &gl.stream);
    }
    gl.stream = 0;
    gl.stream_data = nullptr;
    gl.staging.clear();
}

} // namespace

void InitStream() {
    gl.stream_persistent = (gl.gl_major > 4 || (gl.gl_major == 4 && gl.gl_minor >= 4) ||
                            HasExtension("GL_ARB_buffer_storage"));
    CreateStream(kInitialSegment);
}

void ShutdownStream() { DestroyStream(); }

GLuint StreamBuffer() { return gl.stream; }

StreamSpan StreamAllocate(size_t size, size_t alignment) {
    GLsizeiptr segment = SegmentSize();
    GLsizeiptr offset = (gl.segment_used + static_cast<GLsizeiptr>(alignment) - 1) /
                        static_cast<GLsizeiptr>(alignment) * static_cast<GLsizeiptr>(alignment);
    if (offset + static_cast<GLsizeiptr>(size) > segment) {
        // A frame that outgrows its segment: everything is drawn so far, and the buffer is remade
        // twice as large (or as large as the request) for the rest of the frame and the frames after.
        StreamFlush();
        glFinish();
        GLsizeiptr grown = std::max(segment * 2, static_cast<GLsizeiptr>(size) * 2);
        DestroyStream();
        CreateStream(grown);
        segment = SegmentSize();
        offset = 0;
    }
    gl.segment_used = offset + static_cast<GLsizeiptr>(size);
    GLsizeiptr base = static_cast<GLsizeiptr>(gl.segment) * segment;
    StreamSpan span;
    span.offset = base + offset;
    span.data = gl.stream_persistent ? gl.stream_data + span.offset : gl.staging.data() + offset;
    return span;
}

void StreamFlush() {
    if (gl.stream_persistent || gl.segment_used <= gl.staged_from) {
        return;
    }
    GLsizeiptr base = static_cast<GLsizeiptr>(gl.segment) * SegmentSize();
    glBindBuffer(GL_COPY_WRITE_BUFFER, gl.stream);
    glBufferSubData(GL_COPY_WRITE_BUFFER, base + gl.staged_from, gl.segment_used - gl.staged_from,
                    gl.staging.data() + gl.staged_from);
    glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
    gl.staged_from = gl.segment_used;
}

void StreamEndFrame() {
    StreamFlush();
    GLsync &fence = gl.segment_fences[gl.segment];
    if (fence != nullptr) {
        glDeleteSync(fence);
    }
    fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    gl.segment = (gl.segment + 1) % kStreamSegments;
    gl.segment_used = 0;
    gl.staged_from = 0;
    WaitSegment(gl.segment);
}

} // namespace gfx::detail
