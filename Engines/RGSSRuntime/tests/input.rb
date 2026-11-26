require 'fileutils'
require 'open3'
require 'tmpdir'
require 'zlib'

rgssrun = ARGV[0] or abort 'usage: ruby input.rb path/to/rgssrun'

GAME = <<~'RUBY'
  down = $RGSS_VERSION == 3 ? :DOWN : Input::DOWN
  held = 0
  repeats = []
  120.times do
    Graphics.update
    Input.update
    held = Input.press?(down) ? held + 1 : 0
    repeats << held if Input.repeat?(down)
  end
  text = repeats.join(',')
  $RGSS_VERSION == 3 ? msgbox(text) : print(text)
  exit
RUBY

SCRIPTS = {1 => 'Data/Scripts.rxdata', 3 => 'Data/Scripts.rvdata2'}

def repeats(rgssrun, version)
  Dir.mktmpdir do |dir|
    scripts = SCRIPTS.fetch(version)
    FileUtils.mkdir_p(File.join(dir, 'Data'))
    File.write(File.join(dir, 'Game.ini'), "[Game]\r\nRTP=\r\nScripts=#{scripts.tr('/', '\\')}\r\nTitle=input\r\n")
    code = "$RGSS_VERSION = #{version}\n#{GAME}".b
    File.binwrite(File.join(dir, scripts), Marshal.dump([[1, 'input'.b, Zlib::Deflate.deflate(code).b]]))
    File.write(File.join(dir, 'keys.txt'), "10 down down\n60 up down\n")
    out, status = Open3.capture2e(rgssrun, '--ini', File.join(dir, 'Game.ini'), '--save', File.join(dir, 'save'),
                                  '--frames', '200', '--script', File.join(dir, 'keys.txt'))
    raise "rgssrun failed:\n#{out}" unless status.success?
    out[/^msgbox \d+: (.*)$/, 1].to_s.split(',').map(&:to_i)
  end
end

EXPECTED = {
  1 => [1, 16, 20, 24, 28, 32, 36, 40, 44, 48],
  3 => [1, 24, 30, 36, 42, 48],
}

failures = []
EXPECTED.each do |version, want|
  got = repeats(rgssrun, version)
  if got == want
    puts "PASS RGSS#{version} a held key repeats on the frames RPG Maker's own engine does"
  else
    failures << version
    warn "FAIL RGSS#{version} repeat? on held frames #{got}, want #{want}"
  end
rescue StandardError => error
  failures << version
  warn "FAIL RGSS#{version}: #{error.message}"
end

abort "#{failures.length} failed checks" unless failures.empty?
