import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtCharts

ApplicationWindow {
	id: window
	visible: true
	minimumWidth: 800
	minimumHeight: 500

    // Full screen management
	visibility: ApplicationWindow.Windowed
	Shortcut {
		sequence: "F11"
		onActivated: {
			window.visibility = window.visibility == ApplicationWindow.Windowed ? ApplicationWindow.FullScreen : ApplicationWindow.Windowed
		}
	}

	Shortcut {
		sequence: "Esc"
		onActivated: window.close()
	}

	color: "#11191e"
	title: "MUSIC Azimuth of Arrival Spectrum"

	ColumnLayout {
		height: parent.height
		width: parent.width

		Text {
			Layout.alignment: Qt.AlignCenter
			font.pixelSize: Math.max(24, window.width / 70)
			text: "MUSIC Azimuth Spatial Spectrum"
			color: "#ffffff"
			Layout.margins: 10
		}

		ChartView {
			id: spatialSpectra
			Layout.alignment: Qt.AlignTop
			legend.visible: false
			legend.labelColor: "#e0e0e0"
			Layout.fillWidth: true
			Layout.fillHeight: true
			antialiasing: true
			backgroundColor: "#151f26"

			axes: [
				ValueAxis {
					id: musicSpectrumXAxis

					min: -80
					max: 80
					titleText: "<font color=\"#e0e0e0\">Scanning Vector Angle &Theta; [degrees]</font>"
					titleFont.bold: false
					gridLineColor: "#c0c0c0"
					tickInterval: 30
					tickType: ValueAxis.TicksDynamic
					labelsColor: "#e0e0e0"
				},
				ValueAxis {
					id: musicSpectrumYAxis

					min: -10
					max: 0
					titleText: "<font color=\"#e0e0e0\">P<sub>MUSIC</sub>(&Theta;) [dB]</font>"
					titleFont.bold: false
					gridLineColor: "#c0c0c0"
					tickInterval: 2
					tickType: ValueAxis.TicksDynamic
					labelsColor: "#e0e0e0"
				}
			]

			LineSeries {
				name: "MUSIC Spectrum"
				id: musicSpectrum
				axisX: musicSpectrumXAxis
				axisY: musicSpectrumYAxis

				Component.onCompleted : {
						for (const angle of backend.scanningAngles) {
							musicSpectrum.append(angle, 0)
						}
				}
			}

			Timer {
				interval: 1 / 60 * 1000
				running: true
				repeat: true
				onTriggered: {
					backend.updateSpatialSpectrum(musicSpectrum, musicSpectrumYAxis)
				}
			}
		}
	}
}