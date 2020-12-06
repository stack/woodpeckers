#!/usr/bin/env ruby

require 'fileutils'
require 'optparse'

BUILD_DIRECTORY = 'Build'
VALID_PLATFORMS = [ 'Linux', 'macOS' ].freeze

#
# Parse Options
#

options = {
  clean: false,
  debug: false,
}

options[:platform] = case RUBY_PLATFORM
                     when /linux/
                       'Linux'
                     when /darwin/
                       'macOS'
                     else
                       nil
                     end

OptionParser.new do |opts|
  opts.banner = "Usage: prepare.rb [options]"

  opts.on('-c', '--clean', 'Fully clean before preparing') do
    options[:clean] = true
  end

  opts.on('-d', '--[no-]debug', 'Prepare a debug build') do |d|
    options[:debug] = d
  end

  opts.on('-h', '--help', 'Print this help message') do
    puts opts
    exit
  end

  opts.on('-pPLATFORM', '--platform=PLATFORM', 'The platform to prepare for') do |p|
    options[:platform] = p
  end
end.parse!

#
# Validte Options
#

if options[:platform].nil?
  raise 'A platform could not be determined'
end

unless VALID_PLATFORMS.include? options[:platform]
  raise "Invalid platform: #{options[:platform]}"
end

#
# Prepare the build directory
#

if options[:clean]
  FileUtils.rm_rf BUILD_DIRECTORY
end

unless Dir.exists? BUILD_DIRECTORY
  FileUtils.mkdir_p BUILD_DIRECTORY
end

#
# Run the configuration
#

Dir.chdir BUILD_DIRECTORY do
  args = [
    'cmake',
    '-G',
    'Ninja',
    '..',
    '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
    "-DTARGET_PLATFORM=#{options[:platform]}"
  ]

  if options[:debug]
    args << '-DCMAKE_BUILD_TYPE=Debug'
  else
    args << '-DCMAKE_BUILD_TYPE=Release'
  end

  unless system(*args)
    raise 'Failed to prepare the build'
  end
end
