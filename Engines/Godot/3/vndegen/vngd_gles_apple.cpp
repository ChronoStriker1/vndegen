// Godot 3's renderers call two of Apple's OpenGL ES extensions on iOS, for multisampling, which MetalANGLE doesn't
// have. These do the same with the OpenGL ES 3 functions it does have.
#if defined(VNDEGEN_ANGLE)
#include <GLES3/gl3.h>

extern "C" void glRenderbufferStorageMultisampleAPPLE(GLenum p_target, GLsizei p_samples, GLenum p_format, GLsizei p_width, GLsizei p_height) {
	glRenderbufferStorageMultisample(p_target, p_samples, p_format, p_width, p_height);
}

// Resolves the read framebuffer into the draw framebuffer, at the size of the read framebuffer's color buffer.
extern "C" void glResolveMultisampleFramebufferAPPLE(void) {
	GLint type = 0, name = 0, width = 0, height = 0;
	glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &type);
	glGetFramebufferAttachmentParameteriv(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &name);
	if (type == GL_RENDERBUFFER) {
		GLint previous = 0;
		glGetIntegerv(GL_RENDERBUFFER_BINDING, &previous);
		glBindRenderbuffer(GL_RENDERBUFFER, name);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
		glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
		glBindRenderbuffer(GL_RENDERBUFFER, previous);
	}
	if (width > 0 && height > 0) {
		glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
}
#endif
