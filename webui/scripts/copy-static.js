const fs = require("fs");
const path = require("path");

function copyFile(source, target) {
    fs.mkdirSync(path.dirname(target), { recursive: true });
    fs.copyFileSync(source, target);
}

function copyDirectory(source, target) {
    if(!fs.existsSync(source)) {
        return;
    }
    for(const entry of fs.readdirSync(source)) {
        const srcPath = path.join(source, entry);
        const dstPath = path.join(target, entry);
        const stats = fs.statSync(srcPath);
        if(stats.isDirectory()) {
            copyDirectory(srcPath, dstPath);
        } else {
            copyFile(srcPath, dstPath);
        }
    }
}

function main() {
    const dist = path.join(__dirname, "..", "dist");
    const pub = path.join(__dirname, "..", "public");
    copyDirectory(pub, dist);
}

main();
