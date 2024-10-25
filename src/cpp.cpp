#include "cpp.h"

Cpp::Cpp() {
}

Cpp::~Cpp() {
}

int Cpp::install(std::string package) {
    Package pkg = this->toolkit->parsePackage(package);
    PackagePaths pkgPath = this->toolkit->getPackagePaths(this->progLang, pkg.name, pkg.version);
    this->toolkit->installOwnDatabase(this->progLang, this->gitRepo);
    
    if(std::filesystem::exists(pkgPath.packageVersionPath + "/include") && std::filesystem::exists(pkgPath.packageVersionPath + "/cpp")
        && std::filesystem::is_directory(pkgPath.packageVersionPath + "/include") && std::filesystem::is_directory(pkgPath.packageVersionPath + "/cpp")) {
        return 0;
    }
    
    if(!std::filesystem::exists(pkgPath.packageBasePath)) {
        std::filesystem::create_directories(pkgPath.packageBasePath);
    }
    
    if(std::filesystem::exists(pkgPath.packageRawPath)) {
        return this->createNewVersion(&pkg, &pkgPath);
    }
    
    std::string repo = this->toolkit->getRepoFromDatabase(this->progLang, pkg.name);
    
    if(repo == "") {
        std::cerr << "No package found with name " << pkg.name << std::endl;
        return 1;
    }

    Downloader downloader;
    if(downloader.downloadGit(repo, pkgPath.packageRawPath) != 0) {
        std::cerr << "Failed to download package " << pkg.name << std::endl;
        return 1;
    }

    return this->createNewVersion(&pkg, &pkgPath);
}

int Cpp::install(std::vector<std::string> packages) {
    int result = 0;
    for(std::string package : packages) {
        result += this->install(package);
    }
    return result;
}

int Cpp::uninstall(std::string package) {
    Package pkg          = this->toolkit->parsePackage(package);
    PackagePaths pkgPath = this->toolkit->getPackagePaths(this->progLang, pkg.name, pkg.version);
    
    return this->toolkit->uninstallPackage(this->progLang, pkg.name, pkg.version);
}

int Cpp::uninstall(std::vector<std::string> packages) {
    return this->toolkit->uinistallAllPackages(this->progLang);
}

int Cpp::update(std::string package) {
    Package pkg = this->toolkit->parsePackage(package);
    
    return this->toolkit->updatePackage(this->progLang, pkg.name);
}

int Cpp::update(std::vector<std::string> packages) {
    return this->toolkit->updateAllPackages(this->progLang);
}

int Cpp::update() {
    return this->toolkit->updateAllPackages(this->progLang);
}

int Cpp::search(std::string package) {
    this->toolkit->getPackagePaths(this->progLang, package, "");
    this->toolkit->installOwnDatabase(this->progLang, this->gitRepo);
    return this->toolkit->searchPackageDatabase(this->progLang, package);
}

int Cpp::list() {
    return this->toolkit->listInstalledPackages(this->progLang);
}

int Cpp::info(std::string package) {
    return this->toolkit->infoPackage(this->progLang, package);
}

void Cpp::setToolkit(PackageManagerToolkit * toolkit) {
    this->toolkit = toolkit;
}

int Cpp::createNewVersion(Package * pkg, PackagePaths * pkgPath) {
    std::string gitCommand = "git -C " + pkgPath->packageRawPath + " checkout ";
    
    if(pkg->isHash) gitCommand += pkg->version;
    else gitCommand += "tags/" + pkg->version;
    
    if(system(gitCommand.c_str()) != 0) {
        std::cerr << "Failed to checkout version " << pkg->version << " for package " << pkg->name << std::endl;
        return 1;
    }
    
    std::string pathToBuild = pkgPath->packageBasePath + "/build";
    
    std::filesystem::create_directories(pkgPath->packageVersionPath);
    std::filesystem::create_directories(pkgPath->packageVersionPath + "/include");
    std::filesystem::create_directories(pkgPath->packageVersionPath + "/cpp");
    std::filesystem::create_directories(pathToBuild);
    
    if(std::filesystem::exists(pkgPath->packageRawPath + "/include")) {
        std::filesystem::copy(pkgPath->packageRawPath + "/include", pkgPath->packageVersionPath + "/include", std::filesystem::copy_options::recursive);
    } else {
        for(const auto & entry : std::filesystem::directory_iterator(pkgPath->packageRawPath)) {
            if(entry.path().extension() == ".h" || entry.path().extension() == ".hpp") {
                std::filesystem::copy(entry.path(), pkgPath->packageVersionPath + "/include");
            }
        }
    }
    
    std::string makeBuild;
    
    if(std::filesystem::exists(pkgPath->packageBasePath + "/CMakeLists.text")) {
        
        std::string cmakeBuild = "cmake -S " + pkgPath->packageRawPath + " -B " + pathToBuild;

        if(system(cmakeBuild.c_str()) != 0) {
            std::cerr << "Failed to generate build files for package " << pkg->name << std::endl;
            std::filesystem::remove_all(pkgPath->packageVersionPath);
            std::filesystem::remove_all(pkgPath->packageBasePath + "/build");
            return 1;
        }

        makeBuild = "make -C " + pathToBuild;
    } else {
        makeBuild = "make -C " + pkgPath->packageRawPath;
    }
    
    if(system(makeBuild.c_str()) != 0) {
        std::cerr << "Failed to build package " << pkg->name << std::endl;
        std::filesystem::remove_all(pkgPath->packageVersionPath);
        std::filesystem::remove_all(pathToBuild);
        return 1;
    }
    
    
    std::string libraryFile;
    std::string libraryExtention;

    for(const auto & entry : std::filesystem::directory_iterator(pathToBuild)) {
        libraryExtention = entry.path().extension();
        if(libraryExtention == ".a" || libraryExtention == ".so" || libraryExtention == ".dll" || libraryExtention == ".lib" || libraryExtention == ".dylib") {
            libraryFile = entry.path();
            break;
        }
    }
    
    if(libraryFile == "") {
        std::cerr << "No library file found for package " << pkg->name << std::endl;
        std::filesystem::remove_all(pkgPath->packageVersionPath);
        std::filesystem::remove_all(pathToBuild);
        system(("git -C " + pkgPath->packageRawPath + " restore .").c_str());
        return 1;
    }

    std::filesystem::copy(libraryFile, pkgPath->packageVersionPath + "/cpp/" + pkg->name + libraryExtention);
    
    std::filesystem::remove_all(pathToBuild);
    system(("git -C " + pkgPath->packageRawPath + " restore .").c_str());
    return 0;
}


extern "C" PackageManager * create() {
    return new Cpp();
}

extern "C" void destroy(PackageManager * pm) {
    delete pm;
}