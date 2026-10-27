# Some games name a DLL by its path, such as "System\name". mkxp-z's Win32API emulation looks the DLL up as
# a Ruby constant, which a path is not, so it gets the file name alone.
class Win32API
  alias_method :vndegen_initialize, :initialize
  def initialize(dll, func, *args)
    vndegen_initialize(dll.to_s.split(/[\\\/]/).last, func, *args)
  end
end

# The libretro core traps instead of raising when load_data opens a missing file, and games rescue Errno::ENOENT from
# load_data to treat optional data files as absent. Raise it before the core opens the file.
module Kernel
  alias_method :vndegen_load_data, :load_data
  private def load_data(name, *args)
    raise Errno::ENOENT, name.to_s unless File.exist?(name.to_s)
    vndegen_load_data(name, *args)
  end
end

# Japanese games convert between Shift-JIS and UTF-8 through kernel32's MultiByteToWideChar and WideCharToMultiByte.
# Both follow Windows: a length of -1 means up to and including the terminating NUL, which a Ruby string passed to
# Windows always has after its bytes, a zero output size asks for the length the result needs, and an output buffer
# too small for the result fails with 0.
module Win32API_Impl
  module Kernel32
    CODE_PAGES = {0 => "Windows-31J", 1 => "Windows-31J", 932 => "Windows-31J", 1252 => "Windows-1252", 65001 => "UTF-8"}

    # The input bytes, ending in `terminator` when the length is -1.
    def self.input(str, len, terminator)
      bytes = str.to_s.b
      return bytes[0, len.to_i * terminator.bytesize] if len.to_i >= 0
      unit = terminator.bytesize
      (0...bytes.bytesize).step(unit) { |at| return bytes[0, at + unit] if bytes[at, unit] == terminator }
      bytes[0, bytes.bytesize - bytes.bytesize % unit] + terminator
    end

    def self.output(dst, size, result, unit, encoding)
      count = result.bytesize / unit
      return count if size.to_i == 0 || !dst.is_a?(String)
      return 0 if count > size.to_i || dst.bytesize < count * unit
      dst.force_encoding(Encoding::BINARY)
      dst[0, count * unit] = result
      dst.force_encoding(encoding)
      count
    end

    class MultiByteToWideChar
      def call(args)
        code_page, _flags, src, src_len, dst, dst_size = args
        encoding = CODE_PAGES.fetch(code_page.to_i, "Windows-31J")
        wide = Kernel32.input(src, src_len, "\0".b).force_encoding(encoding).encode("UTF-16LE", invalid: :replace, undef: :replace).b
        Kernel32.output(dst, dst_size, wide, 2, Encoding::BINARY)
      end
    end

    class WideCharToMultiByte
      def call(args)
        code_page, _flags, src, src_len, dst, dst_size = args
        encoding = CODE_PAGES.fetch(code_page.to_i, "Windows-31J")
        wide = Kernel32.input(src, src_len, "\0\0".b)
        narrow = wide.force_encoding("UTF-16LE").encode(encoding, invalid: :replace, undef: :replace).b
        Kernel32.output(dst, dst_size, narrow, 1, encoding)
      end
    end
  end
end
