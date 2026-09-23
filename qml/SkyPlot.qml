import QtQuick

Canvas {
    id: sky
    property var trajectory: []
    property var observation: ({})
    property real selectedTime: 0
    property real passTime: 0
    property real minimumElevation: 10
    property color gridColor: "#d6dfe1"
    property color textColor: "#263135"
    property color pastColor: "#b77c44"
    property color futureColor: "#087e6f"
    property var directions: [qsTr("北"), qsTr("南"), qsTr("西"), qsTr("东")]
    readonly property var passSamples: {
        let nearest = -1, distance = Infinity;
        for (let i = 0; i < trajectory.length; ++i) {
            if (passTime > 0 && trajectory[i].elevation >= minimumElevation && Math.abs(trajectory[i].time - passTime) < distance) {
                nearest = i;
                distance = Math.abs(trajectory[i].time - passTime);
            }
        }
        if (nearest < 0) return [];
        let start = nearest, end = nearest;
        while (start > 0 && trajectory[start - 1].elevation >= 0 && trajectory[start].time - trajectory[start - 1].time <= 31) --start;
        while (end + 1 < trajectory.length && trajectory[end + 1].elevation >= 0 && trajectory[end + 1].time - trajectory[end].time <= 31) ++end;
        return trajectory.slice(start, end + 1);
    }
    onPassSamplesChanged: requestPaint()
    onDirectionsChanged: requestPaint()
    onTrajectoryChanged: requestPaint()
    onObservationChanged: requestPaint()
    onSelectedTimeChanged: requestPaint()
    onGridColorChanged: requestPaint()
    onTextColorChanged: requestPaint()
    onMinimumElevationChanged: requestPaint()
    onPassTimeChanged: requestPaint()
    onPaint: {
        const ctx = getContext("2d");
        ctx.clearRect(0, 0, width, height);
        const cx = width / 2, cy = height / 2, radius = Math.min(width, height) / 2 - 23;
        function point(azimuth, elevation) {
            const r = radius * (90 - elevation) / 90, a = azimuth * Math.PI / 180;
            return {
                x: cx + r * Math.sin(a),
                y: cy - r * Math.cos(a)
            };
        }
        ctx.strokeStyle = gridColor;
        ctx.lineWidth = 1;
        for (let i = 1; i <= 3; ++i) {
            ctx.beginPath();
            ctx.arc(cx, cy, radius * i / 3, 0, Math.PI * 2);
            ctx.stroke();
        }
        ctx.beginPath();
        ctx.moveTo(cx - radius, cy);
        ctx.lineTo(cx + radius, cy);
        ctx.moveTo(cx, cy - radius);
        ctx.lineTo(cx, cy + radius);
        ctx.stroke();
        ctx.fillStyle = textColor;
        ctx.font = "11px 'Microsoft YaHei UI'";
        ctx.textAlign = "center";
        ctx.fillText(directions[0], cx, cy - radius - 8);
        ctx.fillText(directions[1], cx, cy + radius + 17);
        ctx.fillText(directions[2], cx - radius - 14, cy + 4);
        ctx.fillText(directions[3], cx + radius + 14, cy + 4);
        ctx.textAlign = "left";
        ctx.fillText("30°", cx + 4, cy - radius * 2 / 3 + 12);
        ctx.fillText("60°", cx + 4, cy - radius / 3 + 12);
        if (passSamples.length > 1) {
            for (let i = 1; i < passSamples.length; ++i) {
                const a = passSamples[i - 1], b = passSamples[i], p = point(a.azimuth, a.elevation), q = point(b.azimuth, b.elevation);
                ctx.strokeStyle = b.time < selectedTime ? pastColor : futureColor;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(p.x, p.y);
                ctx.lineTo(q.x, q.y);
                ctx.stroke();
            }
        }
        if (observation.elevation !== undefined && observation.elevation >= 0) {
            const p = point(observation.azimuth, observation.elevation);
            ctx.fillStyle = futureColor;
            ctx.beginPath();
            ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
            ctx.fill();
        }
    }
}
