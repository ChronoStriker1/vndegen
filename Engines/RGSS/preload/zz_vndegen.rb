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

# Windows ignores case in file names, so a game that globs for "Save*.rvdata2" expects to find "save01.rvdata2". Ruby
# ignores File::FNM_CASEFOLD in Dir.glob outside Windows, so each ASCII letter in a name with a wildcard becomes a set
# of both its cases. Names without a wildcard need nothing, since VNDegen's file layer finds them in any case.
class Dir
  def self.vndegen_casefold(pattern)
    path = File.path(pattern)
    return path unless path.valid_encoding?
    path.split("/", -1).map do |name|
      next name unless name.include?("*") || name.include?("?") || name.include?("[")
      escaped = in_set = false
      name.each_char.map do |c|
        letter = !escaped && !in_set && (c.between?("A", "Z") || c.between?("a", "z"))
        if escaped then escaped = false
        elsif c == "\\" then escaped = true
        elsif in_set then in_set = c != "]"
        elsif c == "[" then in_set = true
        end
        letter ? "[#{c.downcase}#{c.upcase}]" : c
      end.join
    end.join("/")
  end

  class << self
    alias_method :vndegen_glob, :glob
    def glob(pattern, *args, **options, &block)
      patterns = pattern.is_a?(Array) ? pattern.map { |p| vndegen_casefold(p) } : vndegen_casefold(pattern)
      vndegen_glob(patterns, *args, **options, &block)
    end

    def [](*patterns, **options)
      vndegen_glob(patterns.map { |p| vndegen_casefold(p) }, **options)
    end
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

# The Japanese RPG Maker XP player (RGSS10xJ.dll) draws text in ＭＳ Ｐゴシック unless a game names another font, and
# the English one (RGSS10xE.dll) in Arial, mkxp-z's default for XP. Game.ini names the player. VNDegen's font table
# gives both their substitutes.
begin
  if File.binread("Game.ini") =~ /^\s*Library\s*=\s*(?:System[\\\/])?RGSS1\d\dJ\.dll/i
    Font.default_name = "ＭＳ Ｐゴシック"
  end
rescue SystemCallError
end
