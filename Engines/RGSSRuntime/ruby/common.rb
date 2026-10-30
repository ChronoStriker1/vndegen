# What every RGSS version shares: running the game's script sections, describing errors the way RGSS does, and the
# Ruby 1.8 and 1.9 behavior the games' scripts expect from today's Ruby. $RGSS_VERSION is 1 (XP), 2 (VX) or 3 (VX Ace).

# Ruby 1.8, which XP and VX run, wrote strings without an encoding. They come back as binary; the games' text is UTF-8.
def rgss_tag_utf8(object)
  if object.is_a?(String) && object.encoding == Encoding::BINARY && !object.frozen?
    object.force_encoding(Encoding::UTF_8)
    object.force_encoding(Encoding::BINARY) unless object.valid_encoding?
  end
  object
end

def rgss_marshal_load(data)
  return Marshal.load(data) if $RGSS_VERSION == 3
  Marshal.load(data, method(:rgss_tag_utf8).to_proc)
end

module Marshal
  class << self
    alias_method :rgss_load, :load unless method_defined?(:rgss_load)

    # Saves and data the game reads itself through Marshal get the same treatment as load_data.
    def load(source, proc = nil, **options)
      return rgss_load(source, proc, **options) if $RGSS_VERSION == 3
      tagging = proc ? ->(object) { proc.call(rgss_tag_utf8(object)) } : ->(object) { rgss_tag_utf8(object) }
      rgss_load(source, tagging, **options)
    end
  end
end

# The script sections, in order. Their names show in errors, so each runs under a file name that indexes them.
$RGSS_SCRIPTS = []

def rgss_run_scripts(path)
  require 'zlib'
  sections = load_data(path)
  $RGSS_SCRIPTS = sections.map { |section| section[1].to_s.dup.force_encoding(Encoding::UTF_8) }
  sections.each_with_index do |section, index|
    code = Zlib::Inflate.inflate(section[2]).force_encoding(Encoding::UTF_8)
    code = rgss_compatible_source(code)
    eval(code, TOPLEVEL_BINDING, format('{%04d}', index), 1)
  end
end

# Ruby 1.8 syntax today's Ruby rejects. `when 1: x` is `when 1 then x`.
def rgss_compatible_source(code)
  return code if $RGSS_VERSION == 3
  code.gsub(/^(\s*when\s+[^\n#]*?[^:\s]):(?=[ \t]|$)/) { "#{$1} then" }
end

# "Script 'Game_Map' line 42: NoMethodError occurred." and the message, as RGSS reports an error.
def rgss_describe_error(error)
  location = (error.backtrace || []).find { |line| line =~ /\A\{\d{4}\}:\d+/ }
  name = 'unknown'
  line = 0
  if location =~ /\A\{(\d{4})\}:(\d+)/
    name = $RGSS_SCRIPTS[$1.to_i] || name
    line = $2.to_i
  elsif error.is_a?(SyntaxError) && error.message =~ /\{(\d{4})\}:(\d+)/
    name = $RGSS_SCRIPTS[$1.to_i] || name
    line = $2.to_i
  end
  message = error.message.to_s.dup.force_encoding(Encoding::UTF_8).scrub
  "Script '#{name}' line #{line}: #{error.class} occurred.\n\n#{message}\n#{(error.backtrace || []).first(8).join("\n")}"
end

# MARK: Ruby 1.8 and 1.9 behavior

Fixnum = Integer unless defined?(Fixnum)
Bignum = Integer unless defined?(Bignum)
TRUE = true unless defined?(TRUE)
FALSE = false unless defined?(FALSE)
NIL = nil unless defined?(NIL)

# Methods Ruby has since removed, for scripts written against 1.8 and 1.9.
module RGSSCompat
  def self.add(mod, name, &body)
    mod.send(:define_method, name, &body) unless mod.method_defined?(name) || mod.private_method_defined?(name)
  end
end

RGSSCompat.add(Object, :=~) { |_other| nil }
%i[taint untaint trust untrust].each { |name| RGSSCompat.add(Object, name) { self } }
%i[tainted? untrusted?].each { |name| RGSSCompat.add(Object, name) { false } }
RGSSCompat.add(File.singleton_class, :exists?) { |path| File.exist?(path) }
RGSSCompat.add(Dir.singleton_class, :exists?) { |path| Dir.exist?(path) }
RGSSCompat.add(Hash, :index) { |value| key(value) }

# RGSS has no standard library on disk. Requests for libraries it built in, or that Ruby 1.8 had, succeed quietly.
module Kernel
  RGSS_BUILT_IN = %w[zlib jcode Win32API win32api dl nkf kconv rbconfig enumerator rational complex].freeze

  alias_method :rgss_require, :require unless private_method_defined?(:rgss_require)
  private :rgss_require

  def require(name)
    rgss_require(name)
  rescue LoadError
    base = File.basename(name.to_s, '.*')
    return false if RGSS_BUILT_IN.any? { |known| known.casecmp?(base) }
    raise
  end
  private :require
end

# MARK: Files

# On PC the game's folder is the working directory. Relative paths read from the save folder, then the game folder,
# whatever their case, and write to the save folder (rgss_game_path).
module RGSSFiles
  def self.relative?(path) = path.is_a?(String) && !path.empty? && !path.start_with?('/')

  def self.read_path(path) = relative?(path) ? Kernel.rgss_game_path(path.tr('\\', '/'), false) : path

  def self.write_path(path)
    return path unless relative?(path)
    out = Kernel.rgss_game_path(path.tr('\\', '/'), true)
    folder = File.dirname(out)
    Dir.mkdir(folder) unless Dir.exist?(folder)
    out
  end

  def self.writing?(mode) = mode.is_a?(String) ? mode =~ /[wa+]/ : mode.is_a?(Integer) && (mode & (File::WRONLY | File::RDWR)) != 0

  # Matches in the save folder and the game folder, as the relative names a game expects.
  def self.glob(pattern)
    return nil unless relative?(pattern)
    names = Kernel.rgss_game_dirs.uniq.flat_map do |dir|
      Dir.rgss_glob(File.join(dir, pattern.tr('\\', '/')), File::FNM_CASEFOLD).map { |path| path.delete_prefix("#{dir}/") }
    end
    names.uniq.sort
  end
end

class << File
  alias_method :rgss_open, :open unless method_defined?(:rgss_open)
  def open(path, mode = 'r', *rest, **options, &block)
    path = RGSSFiles.writing?(mode) ? RGSSFiles.write_path(path) : RGSSFiles.read_path(path)
    rgss_open(path, mode, *rest, **options, &block)
  end

  %i[exist? file? directory? mtime size read binread readlines].each do |name|
    alias_method :"rgss_#{name}", name unless method_defined?(:"rgss_#{name}")
    define_method(name) { |path, *rest, **options| send(:"rgss_#{name}", RGSSFiles.read_path(path), *rest, **options) }
  end

  alias_method :rgss_delete, :delete unless method_defined?(:rgss_delete)
  def delete(*paths) = rgss_delete(*paths.map { |path| RGSSFiles.read_path(path) })
  alias unlink delete
end

module FileTest
  class << self
    %i[exist? file? directory? size].each do |name|
      alias_method :"rgss_#{name}", name unless method_defined?(:"rgss_#{name}")
      define_method(name) { |path| send(:"rgss_#{name}", RGSSFiles.read_path(path)) }
    end
  end
end

class << Dir
  alias_method :rgss_glob, :glob unless method_defined?(:rgss_glob)
  def glob(pattern, *rest, **options, &block)
    names = RGSSFiles.glob(pattern) || rgss_glob(pattern, *rest, **options)
    block ? names.each(&block) : names
  end
  alias [] glob
end

module Kernel
  private

  def open(path, *rest, **options, &block) = File.open(path, *rest, **options, &block)
end
