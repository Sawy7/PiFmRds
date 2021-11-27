#!/bin/sh

# # Create structure
# mkdir -p pi_fm_rds_1.0.0_armhf/usr/local/bin
# mkdir pi_fm_rds_1.0.0_armhf/DEBIAN
# mkdir -p pi_fm_rds_1.0.0_armhf/usr/lib/systemd/user

# touch pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Package: pifmrds" > pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Version: 1.0.0" >> pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Architecture: armhf" >> pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Depends: libc6 (>= 2.28), libdbus-1-3 (>= 1.9.14), libdbus-glib-1-2 (>= 0.78), libglib2.0-0 (>= 2.28.0), libpulse0 (>= 0.99.1), libsndfile1 (>= 1.0.20)" >> pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Maintainer: Jan Nemec <nemec.jan123@gmail.com>" >> pi_fm_rds_1.0.0_armhf/DEBIAN/control
# echo "Description: Your RPi is now FM transmitter!" >> pi_fm_rds_1.0.0_armhf/DEBIAN/control

# # Post install
# touch pi_fm_rds_1.0.0_armhf/DEBIAN/postinst
# chmod 775 pi_fm_rds_1.0.0_armhf/DEBIAN/postinst
# echo "#!/bin/sh" > pi_fm_rds_1.0.0_armhf/DEBIAN/postinst
# echo "systemctl --user enable pifmrds.service" >> pi_fm_rds_1.0.0_armhf/DEBIAN/postinst
# echo "systemctl --user start pifmrds.service" >> pi_fm_rds_1.0.0_armhf/DEBIAN/postinst

# # Pre remove
# touch pi_fm_rds_1.0.0_armhf/DEBIAN/prerm
# chmod 775 pi_fm_rds_1.0.0_armhf/DEBIAN/prerm
# echo "#!/bin/sh" > pi_fm_rds_1.0.0_armhf/DEBIAN/prerm
# echo "systemctl --user stop pifmrds.service" >> pi_fm_rds_1.0.0_armhf/DEBIAN/prerm
# echo "systemctl --user disable pifmrds.service" >> pi_fm_rds_1.0.0_armhf/DEBIAN/prerm

# Remove working directory
# rm -rf pi_fm_rds_1.0.0_armhf/

# Remove previous deb
rm *.deb

# Perms for install scripts
chmod 775 pi_fm_rds_1.0.0_armhf/DEBIAN/postinst
chmod 775 pi_fm_rds_1.0.0_armhf/DEBIAN/postrm

# Copy execs
sudo cp ../src/pi_fm_rds pi_fm_rds_1.0.0_armhf/usr/local/bin/pifmrds
sudo chown root pi_fm_rds_1.0.0_armhf/usr/local/bin/pifmrds
sudo chmod +s pi_fm_rds_1.0.0_armhf/usr/local/bin/pifmrds
sudo cp ../systemd/pi_fm_runner pi_fm_rds_1.0.0_armhf/usr/local/bin/
sudo chmod 777 pi_fm_rds_1.0.0_armhf/usr/local/bin/pi_fm_runner

# Systemd copying
cp ../systemd/pifmrds.service pi_fm_rds_1.0.0_armhf/usr/lib/systemd/user/

# Make '.deb' file
dpkg-deb --build --root-owner-group pi_fm_rds_1.0.0_armhf

