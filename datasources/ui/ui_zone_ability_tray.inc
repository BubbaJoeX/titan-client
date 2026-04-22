<Page
	BackgroundOpacity='0.00'
	Name='ZoneAbilityTray'
	PackSize='1,1'
	ScrollExtent='1024,768'
	Size='1024,768'
	Visible='false'
>
	<Page
		BackgroundColor='#202020'
		BackgroundOpacity='0.92'
		BackgroundTint='#00354F'
		Location='212,520'
		MinimumSize='600,200'
		Name='zoneAbilityTrayWindow'
		PalBgTint='back1'
		RStyleDefault='/Styles.New.whiteOutline.rs_default'
		ScrollExtent='600,200'
		Size='600,200'
		UserMovable='true'
		UserResizable='false'
	>
		<Data
			Name='CodeData'
			buttonClose='header.buttonClose'
			textTitle='header.textTitle'
			textDesc='content.textDesc'
			buttonSlot0='slots.slot0.button'
			imageSlot0='slots.slot0.image'
			buttonSlot1='slots.slot1.button'
			imageSlot1='slots.slot1.image'
			buttonSlot2='slots.slot2.button'
			imageSlot2='slots.slot2.image'
			buttonSlot3='slots.slot3.button'
			imageSlot3='slots.slot3.image'
			buttonSlot4='slots.slot4.button'
			imageSlot4='slots.slot4.image'
			buttonSlot5='slots.slot5.button'
			imageSlot5='slots.slot5.image'
		/>
		<Page Location='0,0' Name='header' ScrollExtent='600,28' Size='600,28'>
			<Text Enabled='false' Font='bold_14' LocalText='Zone Abilities' Location='8,4' Name='textTitle' ScrollExtent='480,20' Size='480,20' TextAlignment='Left' TextColor='#FFFFFF'>Zone Abilities</Text>
			<Button LocalText='X' Location='572,2' Name='buttonClose' ScrollExtent='24,24' Size='24,24' Style='/Styles.New.buttons.hud.style'>X</Button>
		</Page>
		<Page Location='8,36' Name='content' ScrollExtent='584,156' Size='584,156'>
			<Text Enabled='false' Font='bold_11' LocalText='' Location='0,112' Name='textDesc' ScrollExtent='584,44' Size='584,44' TextAlignment='UpperLeft' TextColor='#CCCCCC'></Text>
			<Page Location='0,0' Name='slots' ScrollExtent='584,104' Size='584,104'>
				<Page Location='0,0' Name='slot0' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' MaximumSize='48,48' MinimumSize='32,32' Name='image' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
				<Page Location='98,0' Name='slot1' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' Name='image' MaximumSize='48,48' MinimumSize='32,32' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
				<Page Location='196,0' Name='slot2' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' Name='image' MaximumSize='48,48' MinimumSize='32,32' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
				<Page Location='294,0' Name='slot3' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' Name='image' MaximumSize='48,48' MinimumSize='32,32' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
				<Page Location='392,0' Name='slot4' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' Name='image' MaximumSize='48,48' MinimumSize='32,32' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
				<Page Location='490,0' Name='slot5' ScrollExtent='92,104' Size='92,104'>
					<Image Location='22,4' Name='image' MaximumSize='48,48' MinimumSize='32,32' ScrollExtent='48,48' Size='48,48'/>
					<Button Location='4,56' Name='button' ScrollExtent='84,40' Size='84,40' Style='/Styles.New.buttons.hud.style'></Button>
				</Page>
			</Page>
		</Page>
	</Page>
</Page>
