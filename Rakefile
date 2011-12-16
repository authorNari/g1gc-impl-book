# -*- coding: utf-8 -*-
require "yaml"

@config = YAML.load_file("config.yml")

def env(key, error_check=false)
  raise "#{key} を指定してください．例: rake task_name #{key}=\"name name\"" if ENV[key].nil? && error_check
  ENV[key]
end

def bin(b)
  ENV["BIN_DIR"] ||= ""
  File.join(ENV["BIN_DIR"], "#{b}")
end

desc "原稿の文字数や行数などを集計する。"
task :count do
  sh(bin("review-vol"))
end

task :default => [:count]

desc "原稿のHTMLを出力"
task :html => [:index_html] do
  sh("#{bin('review-compile')} --target=html --subdirmode -a --check")
  sh("#{bin('review-compile')} --target=html --subdirmode -a")
end

desc "index.htmlをREADME.mdから生成"
task :index_html do
  require "bluecloth"
  require "erb"
  body = BlueCloth.new(File.read("README.md")).to_html
  title = @config['booktitle']
  str = ERB.new(File.read("layouts/index.erb")).result(binding)
  File.open("index.html", "w"){|f| f.write(str)}
end

desc "全htmlを生成"
task :package => [:html] do
  sh("rm -rf pkg") if File.exists?("pkg")
  sh("mkdir pkg")

  Rake::Task[:html].invoke
  Rake::Task[:index_html].invoke

  sh("mv " + Dir.glob("./*.html").join(" ") + " pkg")
  sh("cp -rf ./images pkg")

  common_files = %w(stylesheet.css)
  sh("cp #{common_files.join(" ")} pkg")
end

desc "現在のebookを生成する"
task :ebook do
  bn = @config['bookname']
  d = @config['date'].strftime('%Y%m%d')
  sh("rm -rf #{bn} #{bn}.epub #{bn}.mobi")
  sh("#{bin('review-epubmaker')} config.yml")
  sh("./misc/kindlegen #{bn}.epub")
  sh("mkdir -p ebook")
  sh("mv #{bn}.epub ebook/g1gc-impl-#{d}.epub")
  sh("mv #{bn}.mobi ebook/g1gc-impl-#{d}.mobi")
end

task :clean do
  sh("rm *.html")
  sh("rm -r pkg")
end
