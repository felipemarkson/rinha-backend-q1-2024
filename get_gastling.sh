set -ex

VERSION="3.10.3"
rm -rf gatling/
GATLING="gatling-charts-highcharts-bundle-$VERSION"
wget -q https://repo1.maven.org/maven2/io/gatling/highcharts/gatling-charts-highcharts-bundle/$VERSION/$GATLING-bundle.zip
unzip -q $GATLING-bundle.zip
rm $GATLING-bundle.zip
mv $GATLING/ gatling/