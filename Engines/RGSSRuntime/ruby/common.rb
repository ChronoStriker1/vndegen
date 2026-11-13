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
  $RGSS_SCRIPTS = load_data(path)
  $RGSS_SCRIPTS.each do |section|
    section[1] = section[1].to_s.dup.force_encoding(Encoding::UTF_8)
    code = Zlib::Inflate.inflate(section[2]).force_encoding(Encoding::UTF_8)
    code = code.delete_prefix("\uFEFF")
    section[3] = rgss_compatible_source(code)
  end
  $RGSS_SCRIPTS.each_with_index do |section, index|
    eval(section[3], TOPLEVEL_BINDING, format('{%04d}', index), 1)
  end
end

# Ruby 1.8 syntax today's Ruby rejects. `when 1: x` is `when 1 then x`.
def rgss_compatible_source(code)
  return code if $RGSS_VERSION == 3 || rgss_compiles?(code)
  code.gsub(/^[ \t]*when[ \t][^\n]*/) { |line| rgss_when_then(line) }
end

def rgss_compiles?(code)
  RubyVM::InstructionSequence.compile(code)
  true
rescue SyntaxError
  false
end

def rgss_when_then(line)
  quote = nil
  index = line.index('when') + 4
  while index < line.length
    char = line[index]
    if quote
      if char == '\\' then index += 1
      elsif char == quote then quote = nil
      end
    elsif char == '"' || char == "'" then quote = char
    elsif char == '#' then return line
    elsif char == ':' && line[index - 1] !~ /[:\s]/ && line[index + 1] != ':' && (line[index + 1].nil? || line[index + 1] =~ /[ \t\r]/)
      return "#{line[0...index]} then#{line[index + 1..]}"
    end
    index += 1
  end
  line
end

# "Script 'Game_Map' line 42: NoMethodError occurred." and the message, as RGSS reports an error.
def rgss_describe_error(error)
  message = error.message.to_s.dup.force_encoding(Encoding::UTF_8).scrub
  location = (error.backtrace || []).find { |line| line =~ /\A\{\d{4}\}:\d+/ }
  name = 'unknown'
  line = 0
  if location =~ /\A\{(\d{4})\}:(\d+)/
    name = $RGSS_SCRIPTS.dig($1.to_i, 1) || name
    line = $2.to_i
  elsif error.is_a?(SyntaxError) && message =~ /\{(\d{4})\}:(\d+)/
    name = $RGSS_SCRIPTS.dig($1.to_i, 1) || name
    line = $2.to_i
  end
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
    make_folders(File.dirname(out))
    out
  end

  def self.make_folders(folder)
    return if Dir.rgss_exist?(folder)
    make_folders(File.dirname(folder))
    Dir.rgss_mkdir(folder)
  rescue Errno::EEXIST
    nil
  end

  def self.writing?(mode) = mode.is_a?(String) ? mode =~ /[wa+]/ : mode.is_a?(Integer) && (mode & (File::WRONLY | File::RDWR)) != 0

  def self.updating?(mode)
    return mode.start_with?('a') || (mode.start_with?('r') && mode.include?('+')) if mode.is_a?(String)
    mode.is_a?(Integer) && (mode & (File::WRONLY | File::RDWR)) != 0 && (mode & File::TRUNC) == 0
  end

  def self.mode_of(rest, options) = rest.first.is_a?(String) || rest.first.is_a?(Integer) ? rest.first : options.fetch(:mode, 'r')

  def self.open_path(path, mode)
    return read_path(path) unless writing?(mode)
    out = write_path(path)
    if updating?(mode) && relative?(path) && !File.rgss_exist?(out)
      shipped = read_path(path)
      IO.copy_stream(shipped, out) if shipped != out && File.rgss_file?(shipped)
    end
    out
  end

  # A glob pattern that matches whatever the case of each letter, since Dir.glob ignores File::FNM_CASEFOLD. Letters
  # inside brackets are left alone.
  def self.caseless(pattern)
    depth = 0
    pattern.each_char.map do |char|
      depth += 1 if char == '['
      depth -= 1 if char == ']' && depth > 0
      depth == 0 && char =~ /[A-Za-z]/ ? "[#{char.downcase}#{char.upcase}]" : char
    end.join
  end

  # Matches in the save folder and the game folder, as the relative names a game expects.
  def self.glob(pattern)
    return nil unless relative?(pattern)
    names = Kernel.rgss_game_dirs.uniq.flat_map do |dir|
      Dir.rgss_glob(File.join(dir, caseless(pattern.tr('\\', '/')))).map { |path| path.delete_prefix("#{dir}/") }
    end
    names.uniq.sort
  end

  def self.entries(path)
    return nil unless relative?(path)
    folder = path.tr('\\', '/').chomp('/')
    folders = Kernel.rgss_game_dirs.uniq.flat_map { |dir| Dir.rgss_glob(File.join(dir, caseless(folder))) }
    folders = folders.select { |found| File.rgss_directory?(found) }
    folders.empty? ? nil : folders.flat_map { |found| Dir.rgss_entries(found) }.uniq
  end
end

class << File
  alias_method :rgss_open, :open unless method_defined?(:rgss_open)
  def open(path, *rest, **options, &block)
    rgss_open(RGSSFiles.open_path(path, RGSSFiles.mode_of(rest, options)), *rest, **options, &block)
  end

  alias_method :rgss_new, :new unless method_defined?(:rgss_new)
  def new(path, *rest, **options, &block)
    rgss_new(RGSSFiles.open_path(path, RGSSFiles.mode_of(rest, options)), *rest, **options, &block)
  end

  %i[exist? file? directory? mtime size read binread readlines size? zero? empty? readable? stat lstat atime ctime].each do |name|
    alias_method :"rgss_#{name}", name unless method_defined?(:"rgss_#{name}")
    define_method(name) { |path, *rest, **options| send(:"rgss_#{name}", RGSSFiles.read_path(path), *rest, **options) }
  end

  # Deletes reach only the save folder, so a game cleaning up a failed save never removes a save that came with it.
  alias_method :rgss_delete, :delete unless method_defined?(:rgss_delete)
  def delete(*paths) = rgss_delete(*paths.map { |path| RGSSFiles.relative?(path) ? Kernel.rgss_game_path(path.tr('\\', '/'), true) : path })
  alias unlink delete

  alias_method :rgss_rename, :rename unless method_defined?(:rgss_rename)
  def rename(source, destination) = rgss_rename(RGSSFiles.write_path(source), RGSSFiles.write_path(destination))
end

class << IO
  %i[read binread readlines foreach].each do |name|
    alias_method :"rgss_io_#{name}", name unless method_defined?(:"rgss_io_#{name}")
    define_method(name) { |path, *rest, **options, &block| send(:"rgss_io_#{name}", RGSSFiles.read_path(path), *rest, **options, &block) }
  end

  %i[write binwrite].each do |name|
    alias_method :"rgss_io_#{name}", name unless method_defined?(:"rgss_io_#{name}")
    define_method(name) do |path, data, *rest, **options|
      mode = rest.empty? ? options.fetch(:mode, 'w') : 'r+'
      send(:"rgss_io_#{name}", RGSSFiles.open_path(path, mode), data, *rest, **options)
    end
  end
end

module FileTest
  class << self
    %i[exist? file? directory? size size? zero? empty? readable?].each do |name|
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

  alias_method :rgss_exist?, :exist? unless method_defined?(:rgss_exist?)
  def exist?(path) = rgss_exist?(RGSSFiles.read_path(path))

  alias_method :rgss_mkdir, :mkdir unless method_defined?(:rgss_mkdir)
  def mkdir(path, *rest) = rgss_mkdir(RGSSFiles.write_path(path), *rest)

  alias_method :rgss_rmdir, :rmdir unless method_defined?(:rgss_rmdir)
  def rmdir(path) = rgss_rmdir(RGSSFiles.relative?(path) ? Kernel.rgss_game_path(path.tr('\\', '/'), true) : path)
  alias delete rmdir
  alias unlink rmdir

  alias_method :rgss_entries, :entries unless method_defined?(:rgss_entries)
  def entries(path, **options) = RGSSFiles.entries(path) || rgss_entries(path, **options)

  def children(path, **options) = entries(path, **options) - ['.', '..']

  def each_child(path, **options, &block)
    return enum_for(:each_child, path, **options) unless block
    children(path, **options).each(&block)
    nil
  end

  def foreach(path, **options, &block)
    return enum_for(:foreach, path, **options) unless block
    entries(path, **options).each(&block)
    nil
  end
end

module Kernel
  private

  def open(path, *rest, **options, &block) = File.open(path, *rest, **options, &block)
end
