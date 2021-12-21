Vagrant.configure("2") do |config|

  config.vm.define "server" do |server|
    server.vm.box = "ubuntu/focal64"
    server.vm.provision :shell, path: "bootstrap.sh"
    server.vm.hostname = "server"
    server.vm.network :private_network, ip: "192.168.101.10"
    server.vm.synced_folder "./", "/home/vagrant/os-challenge-common/"
    server.vm.provider :virtualbox do |v|
      v.customize ["modifyvm", :id, "--natdnshostresolver1", "on"]
      v.customize ["modifyvm", :id, "--memory", 512]
      v.customize ["modifyvm", :id, "--cpus", 4]
      v.customize ["modifyvm", :id, "--name", "server"]
    end
  end

  config.vm.define "client" do |client|
    client.vm.box = "ubuntu/focal64"
    client.vm.provision :shell, path: "bootstrap.sh"
    client.vm.hostname = "client"
    client.vm.network :private_network, ip: "192.168.101.11"
    client.vm.synced_folder "./", "/home/vagrant/os-challenge-common/"
    client.vm.provider :virtualbox do |v|
      v.customize ["modifyvm", :id, "--natdnshostresolver1", "on"]
      v.customize ["modifyvm", :id, "--memory", 512]
      v.customize ["modifyvm", :id, "--cpus", 1]
      v.customize ["modifyvm", :id, "--name", "client"]
    end
  end

end
