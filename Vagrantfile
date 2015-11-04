# -*- mode: ruby -*-
# vi: set ft=ruby :

Vagrant.configure(2) do |config|
  config.vm.define "server" do |server|
    server.vm.box = "ten0s/centos6.5_x86_64"
    server.vm.network "private_network", ip: "192.168.200.101"
    server.vm.network "forwarded_port", guest: 22, host: 2501
    server.vm.provider "virtualbox" do |vb|
      vb.customize ["modifyvm", :id, "--memory", "1024"]
    end
  end

  config.vm.define "client" do |client|
    client.vm.box = "ten0s/centos6.5_x86_64"
    client.vm.network "private_network", ip: "192.168.200.102"
    client.vm.network "forwarded_port", guest: 22, host: 2502
    client.vm.provider "virtualbox" do |vb|
      vb.customize ["modifyvm", :id, "--memory", "1024"]
    end
  end

  config.ssh.forward_x11 = true
  config.vbguest.auto_update = false
end
