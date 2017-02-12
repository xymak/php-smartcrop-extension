# php-smartcrop-extension

smartcrop implementation in php-extension.

smartcrop finds optimal crops for images, based on Jonas Wagner's [smartcrop.js](https://github.com/jwagner/smartcrop.js).

![Example](./example.png)

## Installation

Make sure you have compiled PHP7 environment.

Additionally PHP GD extension is needed to be load into PHP.

This PHP extension can only work on Linux OS.

You can install it by running:
```
cd /path/to/php/extention/source/directory
https://github.com/xymak/php-smartcrop-extension.git
cd php-smartcrop-extension
phpize
./configure --prefix='/path/to/php/directory' --with-php-config='/path/to/php/config'
make && make install
echo "extension=smartcrop.so" >> /path/to/etc/php/php.d/smartcorp.ini
```

## Example 
```
<?php
//Convert to a image to gd resource
$im = imagecreatefromstring(file_get_contents('/path/to/a/image'));
//Target Size
$width = 400;
$height = 400;
//Call smartcrop function
$cim = smartcrop($im,$width,$height);
```
